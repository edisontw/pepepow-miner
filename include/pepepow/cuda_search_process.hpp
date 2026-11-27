#pragma once

#include "pepepow/cpu_worker_pool.hpp"
#include "pepepow/cuda_search_result_decoder.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace pepepow {
namespace detail {

class CudaSearchTempDirectory {
 public:
  CudaSearchTempDirectory() {
    std::error_code error;
    const auto root = std::filesystem::temp_directory_path(error);
    if (error) return;
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
      const auto token = sequence_.fetch_add(1, std::memory_order_relaxed);
      const auto candidate = root / ("pepepow-cuda-search-" +
#ifdef _WIN32
          std::to_string(GetCurrentProcessId()) + "-" +
#else
          std::to_string(static_cast<unsigned long>(::getpid())) + "-" +
#endif
          std::to_string(token));
      error.clear();
      if (std::filesystem::create_directory(candidate, error)) {
        path_ = candidate;
        return;
      }
      if (error && error != std::errc::file_exists) return;
    }
  }

  CudaSearchTempDirectory(const CudaSearchTempDirectory&) = delete;
  CudaSearchTempDirectory& operator=(const CudaSearchTempDirectory&) = delete;

  ~CudaSearchTempDirectory() {
    if (path_.empty()) return;
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  explicit operator bool() const { return !path_.empty(); }
  const std::filesystem::path& path() const { return path_; }

 private:
  inline static std::atomic_uint64_t sequence_{0};
  std::filesystem::path path_;
};

inline std::string encode_hex(const std::uint8_t* bytes, std::size_t count) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(count * 2);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(digits[bytes[index] >> 4]);
    result.push_back(digits[bytes[index] & 0x0f]);
  }
  return result;
}

inline std::string path_argument(const std::filesystem::path& value) {
  const auto utf8 = value.u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

#ifdef _WIN32
inline std::optional<std::wstring> utf8_to_wide(std::string_view text) {
  if (text.empty()) return std::wstring{};
  if (text.size() >= 32767) return std::nullopt;
  const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
      text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (required <= 0) return std::nullopt;
  std::wstring result(static_cast<std::size_t>(required), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
      static_cast<int>(text.size()), result.data(), required) != required)
    return std::nullopt;
  return result;
}

inline std::wstring quote_windows_argument(std::wstring_view argument) {
  std::wstring result(1, L'"');
  std::size_t slashes = 0;
  for (const wchar_t character : argument) {
    if (character == L'\\') {
      ++slashes;
    } else if (character == L'"') {
      result.append(slashes * 2 + 1, L'\\');
      result.push_back(L'"');
      slashes = 0;
    } else {
      result.append(slashes, L'\\');
      slashes = 0;
      result.push_back(character);
    }
  }
  result.append(slashes * 2, L'\\');
  result.push_back(L'"');
  return result;
}

class CudaChildProcess {
 public:
  CudaChildProcess() = default;
  CudaChildProcess(const CudaChildProcess&) = delete;
  CudaChildProcess& operator=(const CudaChildProcess&) = delete;
  ~CudaChildProcess() { close(); }

  bool start(const std::filesystem::path& executable,
             const std::vector<std::string>& arguments,
             const std::filesystem::path& output,
             const std::filesystem::path& errors) {
    const auto executable_text = executable.u8string();
    const auto executable_utf8 = std::string(
        reinterpret_cast<const char*>(executable_text.data()), executable_text.size());
    const auto wide_executable = utf8_to_wide(executable_utf8);
    if (!wide_executable || wide_executable->empty()) return false;

    std::wstring command = quote_windows_argument(*wide_executable);
    for (const auto& argument : arguments) {
      const auto wide = utf8_to_wide(argument);
      if (!wide) return false;
      command.push_back(L' ');
      command += quote_windows_argument(*wide);
    }
    if (command.size() >= 32767) return false;

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE output_handle = CreateFileW(output.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (output_handle == INVALID_HANDLE_VALUE) return false;
    HANDLE error_handle = CreateFileW(errors.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (error_handle == INVALID_HANDLE_VALUE) {
      CloseHandle(output_handle);
      return false;
    }
    HANDLE input_handle = CreateFileW(L"NUL", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input_handle == INVALID_HANDLE_VALUE) {
      CloseHandle(output_handle);
      CloseHandle(error_handle);
      return false;
    }

    job_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    const bool job_ready = job_ && SetInformationJobObject(job_,
        JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input_handle;
    startup.StartupInfo.hStdOutput = output_handle;
    startup.StartupInfo.hStdError = error_handle;
    SIZE_T attribute_bytes = 0;
    (void)InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_bytes);
    std::vector<std::byte> attributes(attribute_bytes);
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        attributes.data());
    const bool attributes_initialized = attribute_bytes != 0 &&
        InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0,
                                          &attribute_bytes);
    HANDLE inherited_handles[] = {input_handle, output_handle, error_handle};
    const bool handles_restricted = attributes_initialized &&
        UpdateProcThreadAttribute(startup.lpAttributeList, 0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited_handles,
            sizeof(inherited_handles), nullptr, nullptr);
    std::wstring mutable_command = command;
    const bool created = job_ready && handles_restricted && CreateProcessW(wide_executable->c_str(),
        mutable_command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr, &startup.StartupInfo, &process_);
    if (attributes_initialized)
      DeleteProcThreadAttributeList(startup.lpAttributeList);
    CloseHandle(input_handle);
    CloseHandle(output_handle);
    CloseHandle(error_handle);
    if (!created) {
      if (job_) CloseHandle(job_);
      job_ = nullptr;
      return false;
    }
    if (!AssignProcessToJobObject(job_, process_.hProcess)) {
      (void)TerminateProcess(process_.hProcess, 1);
      (void)WaitForSingleObject(process_.hProcess, 1000);
      close();
      return false;
    }
    if (ResumeThread(process_.hThread) == static_cast<DWORD>(-1)) {
      TerminateJobObject(job_, 1);
      (void)WaitForSingleObject(process_.hProcess, 1000);
      close();
      return false;
    }
    return true;
  }

  std::optional<unsigned long> poll() const {
    if (!process_.hProcess) return 127UL;
    const DWORD state = WaitForSingleObject(process_.hProcess, 0);
    if (state == WAIT_TIMEOUT) return std::nullopt;
    DWORD code = 0;
    if (state != WAIT_OBJECT_0 || !GetExitCodeProcess(process_.hProcess, &code))
      return 127UL;
    return code;
  }

  void terminate() {
    if (!job_) return;
    (void)TerminateJobObject(job_, 1);
    if (process_.hProcess)
      (void)WaitForSingleObject(process_.hProcess, INFINITE);
    wait_for_job_empty();
  }

  void cleanup() { close(); }

 private:
  void wait_for_job_empty() const {
    if (!job_) return;
    const ULONGLONG stop_at = GetTickCount64() + 5000;
    while (GetTickCount64() < stop_at) {
      JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
      if (!QueryInformationJobObject(job_, JobObjectBasicAccountingInformation,
                                     &accounting, sizeof(accounting), nullptr) ||
          accounting.ActiveProcesses == 0)
        return;
      Sleep(10);
    }
  }

  void close() {
    if (job_) {
      (void)TerminateJobObject(job_, 1);
      if (process_.hProcess)
        (void)WaitForSingleObject(process_.hProcess, INFINITE);
      wait_for_job_empty();
    }
    if (process_.hThread) CloseHandle(process_.hThread);
    if (process_.hProcess) CloseHandle(process_.hProcess);
    if (job_) CloseHandle(job_);
    process_ = {};
    job_ = nullptr;
  }

  PROCESS_INFORMATION process_{};
  HANDLE job_ = nullptr;
};
#else
class CudaChildProcess {
 public:
  CudaChildProcess() = default;
  CudaChildProcess(const CudaChildProcess&) = delete;
  CudaChildProcess& operator=(const CudaChildProcess&) = delete;
  ~CudaChildProcess() { close(); }

  bool start(const std::filesystem::path& executable,
             const std::vector<std::string>& arguments,
             const std::filesystem::path& output,
             const std::filesystem::path& errors) {
    const int output_fd = ::open(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (output_fd < 0) return false;
    const int error_fd = ::open(errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (error_fd < 0) {
      ::close(output_fd);
      return false;
    }
    std::vector<std::string> storage;
    storage.reserve(arguments.size() + 1);
    storage.push_back(executable.string());
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (auto& value : storage) argv.push_back(value.data());
    argv.push_back(nullptr);

    pid_ = ::fork();
    if (pid_ == 0) {
      (void)::setpgid(0, 0);
      if (::dup2(output_fd, STDOUT_FILENO) < 0 ||
          ::dup2(error_fd, STDERR_FILENO) < 0) _exit(126);
      ::close(output_fd);
      ::close(error_fd);
      ::execv(executable.c_str(), argv.data());
      _exit(127);
    }
    ::close(output_fd);
    ::close(error_fd);
    if (pid_ < 0) return false;
    (void)::setpgid(pid_, pid_);
    return true;
  }

  std::optional<unsigned long> poll() const {
    if (pid_ <= 0) return std::nullopt;
    siginfo_t info{};
    if (::waitid(P_PID, static_cast<id_t>(pid_), &info,
                 WEXITED | WNOHANG | WNOWAIT) != 0) {
      if (errno == EINTR) return std::nullopt;
      return 127UL;
    }
    if (info.si_pid == 0) return std::nullopt;
    if (info.si_code == CLD_EXITED)
      return static_cast<unsigned long>(info.si_status);
    if (info.si_code == CLD_KILLED || info.si_code == CLD_DUMPED)
      return 128UL + static_cast<unsigned long>(info.si_status);
    return 127UL;
  }

  void terminate() {
    if (pid_ > 0 && !reaped_) (void)::kill(-pid_, SIGTERM);
  }

  void cleanup() { close(); }

 private:
  void close() {
    if (pid_ > 0) {
      // An engine may exit after leaving a child in its process group.
      // Keep the leader unreaped until the group ID is no longer needed.
      (void)::kill(-pid_, SIGKILL);
      if (!reaped_) {
        int status = 0;
        while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {}
      }
    }
    pid_ = -1;
    reaped_ = true;
  }

  pid_t pid_ = -1;
  bool reaped_ = false;
};
#endif

inline bool touch_cancel_file(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  return output.good();
}

inline bool read_bounded_file(const std::filesystem::path& path,
                              std::string& contents) {
  constexpr std::uintmax_t maximum = 1U << 20;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0 || size > maximum) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  contents.resize(static_cast<std::size_t>(size));
  input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
  return input.good() || (input.eof() &&
      static_cast<std::size_t>(input.gcount()) == contents.size());
}

inline std::string read_stderr_excerpt(const std::filesystem::path& path) {
  constexpr std::size_t maximum = 4096;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0) return {};
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  const auto length = static_cast<std::size_t>(
      (std::min<std::uintmax_t>)(size, maximum));
  std::string contents(length, '\0');
  input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
  contents.resize(static_cast<std::size_t>(input.gcount()));
  for (auto& character : contents) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte < 0x20U) character = ' ';
    else if (byte >= 0x7fU) character = '?';
  }
  if (size > maximum) contents += " [stderr truncated]";
  return contents;
}

inline void set_search_diagnostic(std::string* diagnostic,
                                  std::string_view message,
                                  const std::filesystem::path& errors) {
  if (!diagnostic) return;
  *diagnostic = message;
  const auto excerpt = read_stderr_excerpt(errors);
  if (!excerpt.empty()) {
    diagnostic->append(": ");
    diagnostic->append(excerpt);
  }
}

inline std::vector<std::string> make_cuda_search_arguments(
    const Header& source_header, const Digest& target, std::uint32_t start,
    std::uint32_t count, std::uint32_t batch_size, std::uint32_t timeout_ms,
    const std::filesystem::path& cpu_oracle,
    const std::filesystem::path& cancel_file) {
  Header header = source_header;
  pepepow_header_set_nonce(header.data(), header.size(), start);
  return {"--header-hex", encode_hex(header.data(), header.size()),
          "--nonce-start", std::to_string(start),
          "--nonce-count", std::to_string(count),
          "--target-hex", encode_hex(target.data(), target.size()),
          "--batch-size", std::to_string(batch_size),
          "--timeout-ms", std::to_string(timeout_ms),
          "--cpu-oracle", path_argument(cpu_oracle),
          "--cancel-file", path_argument(cancel_file)};
}

}  // namespace detail

struct CudaSearchProcessOptions {
  std::filesystem::path engine;
  std::filesystem::path cpu_oracle;
  std::uint32_t batch_size = 128;
  std::uint32_t timeout_ms = max_search_timeout_ms;
  std::vector<std::string> engine_prefix_arguments;
  std::chrono::milliseconds cancellation_grace{5000};
};

inline SearchResult search_cuda_with_process(
    CpuWorkerPool& cpu, const CudaSearchProcessOptions& options,
    const Header& header, const Digest& target, std::uint32_t start,
    std::uint32_t count, SearchClock::time_point deadline,
    const std::atomic_bool* cancel = nullptr,
    std::string* diagnostic = nullptr) {
  constexpr std::uint32_t maximum_count = 10000000;
  constexpr std::uint64_t nonce_space = std::uint64_t{1} << 32;
  if (diagnostic) diagnostic->clear();
  if (options.engine.empty() || options.cpu_oracle.empty() ||
      options.batch_size == 0 || options.batch_size > 256 ||
      options.timeout_ms == 0 || options.timeout_ms > max_search_timeout_ms ||
      options.cancellation_grace.count() < 0 || count == 0 ||
      count > maximum_count || static_cast<std::uint64_t>(count) > nonce_space - start) {
    if (diagnostic) *diagnostic = "invalid CUDA search request";
    return {SearchStatus::Invalid, std::nullopt};
  }

  const auto cancelled = [cancel] {
    return cancel && cancel->load(std::memory_order_acquire);
  };
  std::uint32_t completed = 0;
  while (completed < count) {
    if (cancelled()) return {SearchStatus::Cancelled, std::nullopt, completed};
    if (SearchClock::now() >= deadline)
      return {SearchStatus::Timeout, std::nullopt, completed};

    const auto nonce = static_cast<std::uint32_t>(start + completed);
    const auto remaining = count - completed;
    const auto invocation_started = SearchClock::now();
    const auto remaining_ms = std::chrono::ceil<std::chrono::milliseconds>(
        deadline - invocation_started);
    const auto invocation_timeout = static_cast<std::uint32_t>((std::clamp)(
        remaining_ms.count(), std::int64_t{1},
        static_cast<std::int64_t>(options.timeout_ms)));

    detail::CudaSearchTempDirectory temporary;
    if (!temporary) return {SearchStatus::Invalid, std::nullopt, completed};
    const auto output_path = temporary.path() / "result.json";
    const auto error_path = temporary.path() / "child.stderr";
    const auto cancel_path = temporary.path() / "cancel.request";
    const auto invalid = [&](std::string_view message) {
      detail::set_search_diagnostic(diagnostic, message, error_path);
      return SearchResult{SearchStatus::Invalid, std::nullopt, completed};
    };
    auto arguments = options.engine_prefix_arguments;
    auto engine_arguments = detail::make_cuda_search_arguments(header, target,
        nonce, remaining, options.batch_size, invocation_timeout,
        options.cpu_oracle, cancel_path);
    arguments.insert(arguments.end(), engine_arguments.begin(), engine_arguments.end());

    detail::CudaChildProcess child;
    if (!child.start(options.engine, arguments, output_path, error_path))
      return invalid("failed to launch CUDA search engine");

    const auto invocation_deadline = (std::min)(deadline,
        invocation_started + std::chrono::milliseconds(invocation_timeout));
    bool requested_cancel = false;
    bool requested_timeout = false;
    std::optional<SearchClock::time_point> cancellation_grace_deadline;
    std::optional<unsigned long> exit_code;
    while (!(exit_code = child.poll())) {
      const auto now = SearchClock::now();
      if (!requested_cancel && cancelled()) {
        requested_cancel = detail::touch_cancel_file(cancel_path);
        if (!requested_cancel) break;
        cancellation_grace_deadline = now + options.cancellation_grace;
      }
      if (!requested_timeout && now >= invocation_deadline) {
        requested_timeout = detail::touch_cancel_file(cancel_path);
        if (!requested_timeout) break;
        if (!cancellation_grace_deadline)
          cancellation_grace_deadline = now + options.cancellation_grace;
      }
      if ((requested_cancel || requested_timeout) && cancellation_grace_deadline) {
        if (now >= *cancellation_grace_deadline) {
          child.terminate();
          (void)child.poll();
          const auto failed_status = requested_timeout && !cancelled()
              ? SearchStatus::Timeout : SearchStatus::Cancelled;
          return {failed_status, std::nullopt, completed};
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!exit_code) return invalid("failed to collect CUDA search engine status");
    child.cleanup();

    std::string json;
    if (!detail::read_bounded_file(output_path, json))
      return invalid("CUDA search engine did not produce bounded result JSON");
    auto result = decode_cuda_search_result(json, header, nonce, remaining);
    if (!result) return invalid("CUDA search engine result failed validation");
    if (*exit_code != 0 && *exit_code != 130)
      return invalid("CUDA search engine exited with code " +
                     std::to_string(*exit_code));
    if (result->hashes > remaining)
      return invalid("CUDA search engine exceeded its requested nonce range");
    completed += result->hashes;

    if (result->status == SearchStatus::Found) {
      if (cancelled()) return {SearchStatus::Cancelled, std::nullopt, completed};
      if (SearchClock::now() >= deadline)
        return {SearchStatus::Timeout, std::nullopt, completed};
      const auto* gpu_candidate = result->candidate ? &*result->candidate : nullptr;
      if (!gpu_candidate)
        return invalid("CUDA search engine omitted its found candidate");
      const auto verified = cpu.search(header, target, gpu_candidate->nonce, 1,
                                       deadline, cancel);
      if (verified.status != SearchStatus::Found || !verified.candidate ||
          verified.candidate->digest != gpu_candidate->digest ||
          verified.candidate->header != gpu_candidate->header) {
        if (verified.status == SearchStatus::Invalid)
          detail::set_search_diagnostic(diagnostic,
              "pinned-Core CPU recheck rejected CUDA candidate", error_path);
        return {verified.status == SearchStatus::Cancelled || cancelled()
                    ? SearchStatus::Cancelled
                    : verified.status == SearchStatus::Timeout ||
                          SearchClock::now() >= deadline
                        ? SearchStatus::Timeout : SearchStatus::Invalid,
                std::nullopt, completed};
      }
      if (cancelled()) return {SearchStatus::Cancelled, std::nullopt, completed};
      if (SearchClock::now() >= deadline)
        return {SearchStatus::Timeout, std::nullopt, completed};
      return {SearchStatus::Found, verified.candidate, completed};
    }
    if (result->status == SearchStatus::Cancelled)
      return {cancelled() ? SearchStatus::Cancelled :
                  requested_timeout || SearchClock::now() >= deadline
                      ? SearchStatus::Timeout : SearchStatus::Cancelled,
              std::nullopt, completed};
    if (result->status == SearchStatus::Timeout) {
      if (SearchClock::now() >= deadline || result->hashes == 0)
        return {SearchStatus::Timeout, std::nullopt, completed};
      continue;
    }
    if (result->status != SearchStatus::Exhausted || result->hashes != remaining)
      return invalid("CUDA search engine returned inconsistent nonce coverage");
  }
  return {SearchStatus::Exhausted, std::nullopt, completed};
}

}  // namespace pepepow
