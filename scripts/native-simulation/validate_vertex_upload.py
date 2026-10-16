"""Small native range/byte checks for the DX11 vertex upload cursor; no GPU or assets."""
import argparse
import os
from pathlib import Path
import subprocess
from run_corpus import ROOT, local_output, file_digest, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    output = local_output(parser.parse_args().output)
    temporary = output / 'tmp'
    temporary.mkdir()
    source = ROOT / 'scripts/native-simulation/native/vertex_upload.cpp'
    header = ROOT / 'libultraship/include/fast/VertexUploadCursor.h'
    setup = Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
    environment = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), UPLOAD_SETUP=str(setup),
        UPLOAD_SOURCE=str(source), UPLOAD_INCLUDE=str(ROOT / 'libultraship/include'),
        UPLOAD_EXE=str(output / 'vertex_upload.exe'), UPLOAD_OBJECT=str(output / 'vertex_upload.obj'))
    wrapper = output / 'run.cmd'
    wrapper.write_text('@echo off\ncall "%UPLOAD_SETUP%" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
        'cl.exe /std:c++20 /EHsc /W4 /WX /O2 /I"%UPLOAD_INCLUDE%" "%UPLOAD_SOURCE%" '
        '/Fe:"%UPLOAD_EXE%" /Fo:"%UPLOAD_OBJECT%"\nif errorlevel 1 exit /b %errorlevel%\n'
        '"%UPLOAD_EXE%"\nexit /b %errorlevel%\n')
    with (output / 'process.log').open('w') as log:
        result = subprocess.run(['cmd.exe', '/c', str(wrapper)], cwd=output, env=environment,
                                stdout=log, stderr=subprocess.STDOUT, timeout=120)
    write_json(output / 'result.json', dict(status='pass' if result.returncode == 0 else 'fail',
        exit_code=result.returncode, source_sha256=file_digest(source), header_sha256=file_digest(header)))
    print((output / 'process.log').read_text())
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
