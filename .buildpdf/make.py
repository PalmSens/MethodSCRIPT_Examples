from datetime import datetime
import subprocess
import argparse
import os

parser = argparse.ArgumentParser(description='Build MethodSCRIPT Example PDFs')
parser.add_argument('--final', action='store_true', help='Save output PDFs to respective example directories')
args = parser.parse_args()

dir_path = os.path.dirname(os.path.realpath(__file__))
os.chdir(dir_path)

EXAMPLES = {
    'arduino': 'Arduino',
    'ccode': 'C',
    'python': 'Python',
}

build_date = datetime.today().strftime('%Y-%m-%d')
process_list = {}

if args.final:
    outputdir = ''
else:
    outputdir = '../.buildpdf/output'

for example, name in EXAMPLES.items():
    os.chdir(f'../Example_{name}')
    command = [
        'asciidoctor-pdf',
        'README.adoc',
        '-a',
        f'revdate={build_date}',
        '-a',
        'pdf-themesdir=../.buildpdf/themes',
        '-a',
        'pdf-theme=palmsens.yml',
        '-a',
        'title-logo-image=../.buildpdf/themes/palmsens/logo_titlepage.png',
        '-a',
        'source-highlighter=rouge',
        '-D',
        f'{outputdir}',
        '-o',
        f'GettingStarted_Example_{name}.pdf',
    ]
    proc = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    process_list[example] = proc

while len(process_list) > 0:
    for example, proc in process_list.items():
        stdout_text = proc.stdout.read1().decode('utf-8')
        stderr_text = proc.stderr.read1().decode('utf-8')
        print(f'Finished {example}')
        if stdout_text:
            print(stdout_text.replace('\r\n', '\n'), end='')
        if stderr_text:
            print(stderr_text.replace('\r\n', '\n'), end='')
        process_list.pop(example)
        break
