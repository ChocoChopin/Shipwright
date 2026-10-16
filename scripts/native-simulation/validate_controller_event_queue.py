"""Native SDL queue/state regression check; process-local virtual pad, no game assets."""
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
    source = ROOT / 'scripts/native-simulation/native/controller_event_queue.cpp'
    header = ROOT / 'libultraship/include/ship/controller/physicaldevice/SDLControllerEventQueue.h'
    sdk = ROOT / 'build/x64/vcpkg/installed/x64-windows-static'
    setup = Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
    environment = dict(os.environ, TEMP=str(temporary), TMP=str(temporary), QUEUE_SETUP=str(setup),
        QUEUE_SOURCE=str(source), QUEUE_INCLUDE=str(ROOT / 'libultraship/include'), QUEUE_SDK=str(sdk),
        QUEUE_EXE=str(output / 'controller_event_queue.exe'), QUEUE_OBJECT=str(output / 'controller_event_queue.obj'))
    wrapper = output / 'run.cmd'
    wrapper.write_text('@echo off\ncall "%QUEUE_SETUP%" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
        'cl.exe /std:c++20 /EHsc /W4 /WX /O2 /MT /I"%QUEUE_INCLUDE%" /I"%QUEUE_SDK%/include" '
        '"%QUEUE_SOURCE%" /Fe:"%QUEUE_EXE%" /Fo:"%QUEUE_OBJECT%" /link '
        '"%QUEUE_SDK%/lib/SDL2-static.lib" kernel32.lib user32.lib gdi32.lib winmm.lib imm32.lib '
        'ole32.lib oleaut32.lib version.lib uuid.lib setupapi.lib shell32.lib advapi32.lib\n'
        'if errorlevel 1 exit /b %errorlevel%\n"%QUEUE_EXE%"\nexit /b %errorlevel%\n')
    with (output / 'process.log').open('w') as log:
        result = subprocess.run(['cmd.exe', '/c', str(wrapper)], cwd=output, env=environment,
                                stdout=log, stderr=subprocess.STDOUT, timeout=120)
    write_json(output / 'result.json', dict(status='pass' if result.returncode == 0 else 'fail',
        exit_code=result.returncode, source_sha256=file_digest(source), header_sha256=file_digest(header)))
    print((output / 'process.log').read_text())
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
