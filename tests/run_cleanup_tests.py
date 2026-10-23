"""Compile the actual production cleanup loop without a game or SKSE runtime."""
import argparse
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='cl')
parser.add_argument('--source', type=Path, default=root / 'src/Serialization.hpp')
parser.add_argument('--asan', action='store_true')
args = parser.parse_args()
source = args.source.read_text(encoding='utf-8-sig')
marker = 'for (RE::FormID headFormID : toRemove)'
assert source.count(marker) == 1, 'Cannot identify the production cleanup loop unambiguously'
start = source.index(marker)
brace = source.index('{', start)
end, depth = brace + 1, 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
loop = source[start:end]
fixture = (root / 'tests/cleanup_fixture.cpp').read_text()
assert fixture.count('// INSERT_PRODUCTION_CLEANUP_LOOP') == 1
with tempfile.TemporaryDirectory(prefix='ngd-cleanup-') as temp:
    directory = Path(temp)
    cpp = directory / 'cleanup.cpp'
    exe = directory / ('cleanup.exe' if Path(args.compiler).stem.lower() == 'cl' else 'cleanup')
    cpp.write_text(fixture.replace('// INSERT_PRODUCTION_CLEANUP_LOOP', loop))
    if Path(args.compiler).stem.lower() == 'cl':
        flags = ['/nologo', '/std:c++20', '/EHsc', '/Zi', '/Od']
        if args.asan: flags.append('/fsanitize=address')
        command = [args.compiler, *flags, str(cpp), '/Fe:' + str(exe),
                   '/Fo:' + str(directory / 'cleanup.obj'), '/Fd:' + str(directory / 'cleanup.pdb')]
    else:
        flags = ['-std=c++20', '-g', '-O1']
        if args.asan: flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [args.compiler, *flags, str(cpp), '-o', str(exe)]
    subprocess.run(command, cwd=directory, check=True)
    result = subprocess.run([str(exe)], cwd=directory)
    if result.returncode:
        raise SystemExit(result.returncode)
