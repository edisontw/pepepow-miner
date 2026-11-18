#!/usr/bin/env python3
"""Compare pinned Core and independent V110 state and stress the original Core on Linux."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes
import hashlib
from itertools import chain
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
from threading import Barrier
from types import SimpleNamespace

from compare_digest_parity import rows

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import g0_v110_independent as independent

CORE_SHA = 'bc08108ceccd957c79040dc7fedc1d56d8d47f97'
OPENLIBM_SHA = '9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d'
DIGEST_TSV_SHA = '73007ec4247215653c54a665dfa8c96f98a08d18593bf36e2d81bfc895c58afb'
BYTE32 = ctypes.c_uint8 * 32
FP64 = struct.Struct('<d')
FP64_ROW = struct.Struct('<64d')
VECTORS = Path(__file__).parent / 'vectors/candidates/explorer-chain-4371217-4371226.jsonl'


class Trace(ctypes.Structure):
    _fields_ = [
        ('first', BYTE32), ('seed', BYTE32), ('mixed', BYTE32), ('digest', BYTE32),
        ('matrix', ctypes.c_double * 4096), ('products', ctypes.c_double * 64),
        ('branches', ctypes.c_uint32 * 12),
        ('singularity', ctypes.c_uint32), ('nonfinite', ctypes.c_uint32),
    ]


TRACE_DECLARATIONS = """
typedef struct {
    uint8_t first[32], seed[32], mixed[32], digest[32];
    double matrix[4096], products[64];
    uint32_t branches[12], singularity, nonfinite;
} g0_trace_result;
static _Thread_local g0_trace_result *g0_trace;
"""


def instrument(source):
    """Insert read-only probes at the existing operations of the pinned C function."""
    hooks = {
        '// Constants': TRACE_DECLARATIONS + '\n// Constants',
        '        return 0; // Avoid singularity':
            '        if (g0_trace) ++g0_trace->singularity;\n'
            '        return 0; // Avoid singularity',
        '    if (transformFactorOne < 0.33) {':
            '    if (g0_trace) {\n'
            '        unsigned family = transformFactorOne < 0.33 ? 0 : transformFactorOne < 0.66 ? 1 : 2;\n'
            '        unsigned quarter = transformFactorTwo < 0.25 ? 0 : transformFactorTwo < 0.5 ? 1 :\n'
            '                           transformFactorTwo < 0.75 ? 2 : 3;\n'
            '        ++g0_trace->branches[4 * family + quarter];\n'
            '    }\n'
            '    if (transformFactorOne < 0.33) {',
        '    while (isnan(transformedValue) || isinf(transformedValue)) {':
            '    while (isnan(transformedValue) || isinf(transformedValue)) {\n'
            '        if (g0_trace) ++g0_trace->nonfinite;',
        '    blake3_hasher_update(&hasher, result, HOOHASH_HASH_SIZE);':
            '    if (g0_trace) {\n'
            '        memcpy(g0_trace->products, product, sizeof product);\n'
            '        memcpy(g0_trace->mixed, result, sizeof result);\n'
            '    }\n'
            '    blake3_hasher_update(&hasher, result, HOOHASH_HASH_SIZE);',
        '    generateHoohashMatrix(matrixSeed, mat);':
            '    generateHoohashMatrix(matrixSeed, mat);\n'
            '    if (g0_trace) {\n'
            '        memcpy(g0_trace->first, firstPass, sizeof firstPass);\n'
            '        memcpy(g0_trace->seed, matrixSeed, sizeof matrixSeed);\n'
            '        memcpy(g0_trace->matrix, mat, sizeof mat);\n'
            '    }',
        'void hoohashv110(': 'static void g0_traced_hoohashv110(',
    }
    for anchor, replacement in hooks.items():
        if source.count(anchor) != 1:
            raise ValueError(f'Unexpected pinned Core trace anchor: {anchor}')
        source = source.replace(anchor, replacement, 1)
    return source + """

void g0_trace_v110(const uint8_t header[80], g0_trace_result *trace) {
    memset(trace, 0, sizeof *trace);
    g0_trace = trace;
    g0_traced_hoohashv110(header, 80, trace->digest);
    g0_trace = NULL;
}
"""


def verify_checkout(path, expected):
    actual = subprocess.check_output(['git', '-C', path, 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(
        ['git', '-C', path, 'status', '--porcelain', '--untracked-files=no'], text=True)
    if actual != expected or dirty:
        raise ValueError(f'Expected clean checkout at {expected}, got {actual}: {path}')


def build_libraries(args, work):
    crypto = args.core / 'src/crypto'
    original_source = crypto / 'hoohash/hoohash.c'
    instrumented = work / 'trace.c'
    instrumented.write_text(instrument(original_source.read_text(encoding='utf-8')), encoding='utf-8')
    flags = ['-O3', '-DNDEBUG'] if args.mode == 'Release' else ['-O0', '-g']
    command = [
        args.cc, '-std=gnu11', '-fPIC', '-shared', '-Wl,-Bsymbolic', *flags,
        '-DHAVE_OPENLIBM=1', '-fno-fast-math', '-ffp-contract=off',
        f'-I{crypto / "hoohash"}', f'-I{args.openlibm / "include"}',
    ]
    blake3 = [crypto / name for name in ('blake3.c', 'blake3_dispatch.c', 'blake3_portable.c')]
    native = work / 'core.so'
    traced = work / 'core-trace.so'
    subprocess.run([*command, original_source, *blake3, args.archive, '-o', native], check=True)
    subprocess.run([
        *command, instrumented, *blake3,
        '-Wl,--whole-archive', args.archive, '-Wl,--no-whole-archive', '-o', traced,
    ], check=True)
    return ctypes.CDLL(str(native)), ctypes.CDLL(str(traced))


def python_trace(header):
    captured = None

    def profile(frame, event, result):
        nonlocal captured
        if event == 'return' and frame.f_code is independent.v110.__code__:
            captured = frame.f_locals.copy()

    previous = sys.getprofile()
    sys.setprofile(profile)
    try:
        digest = independent.v110(header)
    finally:
        sys.setprofile(previous)
    if captured is None:
        raise ValueError('Independent Python trace was not captured')
    return digest, captured


def witness(case, stage, expected, actual):
    name, header, _ = case
    return json.dumps({
        'case': name, 'header': header.hex(), 'nonce_le': int.from_bytes(header[76:], 'little'),
        'stage': stage, 'expected': expected, 'actual': actual,
    }, sort_keys=True)


def require_bytes(case, stage, expected, actual):
    if expected != actual:
        raise ValueError(witness(case, stage, expected.hex(), actual.hex()))


def sample_cases(cases):
    yield from cases[:261]
    prefix = cases[0][1][:76]
    nonces = [*range(257), 0xffffffff, 0x01000000, 0x00010000, 0x80000000]
    for nonce in nonces:
        yield f'nonce-{nonce:08x}', prefix + nonce.to_bytes(4, 'little'), None
    for offset in (0, 4, 35, 36, 67, 68, 71, 72, 75):
        header = bytearray(prefix + bytes(4))
        header[offset] ^= 1
        yield f'field-offset-{offset}', bytes(header), None
    for line in VECTORS.read_text(encoding='utf-8').splitlines():
        row = json.loads(line)
        yield row['case_id'], bytes.fromhex(row['header_hex']), bytes.fromhex(
            row['expected_core_function_output_hex'])


def pinned_math(library):
    funcs = {}
    for name in ('sin', 'cos', 'exp', 'sqrt'):
        function = getattr(library, name)
        function.argtypes = [ctypes.c_double]
        function.restype = ctypes.c_double
        funcs[name] = function
    return SimpleNamespace(floor=math.floor, pi=math.pi, **funcs)


def verify_trace(original_hash, library, sample, math_backend):
    trace_hash = library.g0_trace_v110
    trace_hash.argtypes = [ctypes.c_char_p, ctypes.POINTER(Trace)]
    trace_hash.restype = None
    branches = [0] * 12
    singularity = nonfinite = product_differences = 0
    first_difference = None
    previous = independent.math
    if math_backend == 'openlibm':
        independent.math = pinned_math(library)
    try:
        for case in sample:
            _, header, known = case
            c_trace = Trace()
            trace_hash(header, ctypes.byref(c_trace))
            digest, py = python_trace(header)
            require_bytes(case, 'traced-vs-original', original_hash(header), bytes(c_trace.digest))
            require_bytes(case, 'firstPass', py['first'], bytes(c_trace.first))
            require_bytes(case, 'matrixSeed', py['seed'], bytes(c_trace.seed))
            for i, py_row in enumerate(py['mat']):
                require_bytes(case, f'matrix[{i}]', FP64_ROW.pack(*py_row),
                              FP64_ROW.pack(*c_trace.matrix[i * 64:(i + 1) * 64]))
            for i, (expected, actual) in enumerate(zip(py['product'], c_trace.products, strict=True)):
                if FP64.pack(expected) != FP64.pack(actual):
                    product_differences += 1
                    if first_difference is None:
                        first_difference = witness(case, f'product[{i}]', expected.hex(), actual.hex())
            require_bytes(case, 'pre-final-BLAKE3 bytes', py['intermediate'], bytes(c_trace.mixed))
            require_bytes(case, 'final digest', digest, bytes(c_trace.digest))
            if known is not None:
                require_bytes(case, 'fixed expected digest', known, digest)
            for i, count in enumerate(c_trace.branches):
                branches[i] += count
            singularity += c_trace.singularity
            nonfinite += c_trace.nonfinite
    finally:
        independent.math = previous
    if math_backend == 'openlibm' and first_difference is not None:
        raise ValueError(f'Pinned OpenLibm FP64 product mismatch: {first_difference}')
    print(f'PASS: {len(sample)} traces ({math_backend}); firstPass, matrixSeed, all matrix cells, '
          'pre-final BLAKE3 bytes and full digest are bit-exact')
    print(f'FP64 product differences ({math_backend}): {product_differences}/{len(sample) * 64}')
    if first_difference is not None:
        print(f'First FP64 product difference ({math_backend}): {first_difference}')
    print(f'Core nonlinear branch hits (3 families x 4 quarters): {branches}; '
          f'singularity guard={singularity}; nonfinite recovery iterations={nonfinite}')


def verify_threads(original_hash, cases, workers, rounding_mode):
    barrier = Barrier(workers)
    shared = range(64)

    def exercise(worker):
        if rounding_mode() != 0:
            barrier.abort()
            raise ValueError(f'Worker {worker} must run with FE_TONEAREST')
        barrier.wait(timeout=15)
        count = 0
        for i in chain(shared, range(worker, len(cases), workers)):
            case = cases[i]
            require_bytes(case, f'original Core worker {worker}', case[2], original_hash(case[1]))
            count += 1
        return count

    with ThreadPoolExecutor(max_workers=workers) as pool:
        counts = list(pool.map(exercise, range(workers)))
    print(f'PASS: {sum(counts)} full digests across {workers} native threads; '
          f'{len(cases)} sharded cases plus {len(shared)} common cases per worker')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--openlibm', type=Path, required=True)
    parser.add_argument('--archive', type=Path, required=True, help='Static PIC archive from pinned openlibm')
    parser.add_argument('--python-digests', type=Path, required=True, help='Frozen independent G0 TSV')
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--mode', choices=('Release', 'Debug'), default='Release')
    parser.add_argument('--workers', type=int, choices=range(2, 17), default=8)
    args = parser.parse_args()
    try:
        if sys.platform != 'linux':
            raise ValueError('This trace harness requires Linux and a GNU-compatible linker')
        rounding_mode = ctypes.CDLL(None).fegetround
        rounding_mode.argtypes = []
        rounding_mode.restype = ctypes.c_int
        if rounding_mode() != 0:
            raise ValueError('The trace harness requires FE_TONEAREST')
        verify_checkout(args.core, CORE_SHA)
        verify_checkout(args.openlibm, OPENLIBM_SHA)
        if hashlib.sha256(args.python_digests.read_bytes()).hexdigest() != DIGEST_TSV_SHA:
            raise ValueError('Independent digest TSV does not match the fixed 10000-case checksum')
        cases = [(name, bytes.fromhex(header), bytes.fromhex(digest))
                 for name, header, digest in rows(args.python_digests)]
        sample = list(sample_cases(cases))
        print(f'Mode: {args.mode}; compiler: '
              f'{subprocess.check_output([args.cc, "--version"], text=True).splitlines()[0]}')
        print(f'OpenLibm archive SHA256: {hashlib.sha256(args.archive.read_bytes()).hexdigest()}')
        with TemporaryDirectory(prefix='g0-trace-') as tmp:
            native, traced = build_libraries(args, Path(tmp))
            native.hoohashv110.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint8)]
            native.hoohashv110.restype = None

            def original_hash(header):
                # CDLL releases the GIL for the native call. Each invocation owns its output.
                output = BYTE32()
                native.hoohashv110(header, len(header), output)
                return bytes(output)

            for backend in ('system', 'openlibm'):
                verify_trace(original_hash, traced, sample, backend)
            verify_threads(original_hash, cases, args.workers, rounding_mode)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
