# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import argparse
import json
import os
import re
import subprocess
from collections import namedtuple

from idf_build_apps.constants import SUPPORTED_TARGETS
from pycparser import c_ast, c_parser, preprocess_file

Param = namedtuple('Param', ['ptr', 'array', 'qual', 'type', 'name'])

AUTO_GENERATED = 'This file is auto-generated'
COPYRIGHT_HEADER = open('copyright_header.h', 'r').read()
NAMESPACE = re.compile(r'^esp_wifi')
DEPRECATED_API = ['esp_wifi_set_ant_gpio', 'esp_wifi_get_ant', 'esp_wifi_get_ant_gpio', 'esp_wifi_set_ant']


class FunctionVisitor(c_ast.NodeVisitor):
    def __init__(self, header):
        self.function_prototypes = {}
        self.ptr = 0
        self.array = 0
        self.content = open(header, 'r').read()

    def get_type(self, node, suffix='param'):
        if suffix == 'param':
            self.ptr = 0
            self.array = 0

        if isinstance(node.type, c_ast.TypeDecl):
            typename = node.type.declname
            quals = ''
            if node.type.quals:
                quals = ' '.join(node.type.quals)
            if node.type.type.names:
                type = node.type.type.names[0]
                return quals, type, typename
        if isinstance(node.type, c_ast.PtrDecl):
            quals, type, name = self.get_type(node.type, 'ptr')
            self.ptr += 1
            return quals, type, name

        if isinstance(node.type, c_ast.ArrayDecl):
            quals, type, name = self.get_type(node.type, 'array')
            self.array = int(node.type.dim.value)
            return quals, type, name

    def visit_FuncDecl(self, node):
        if isinstance(node.type, c_ast.TypeDecl):
            func_name = node.type.declname
            if func_name.startswith('esp_wifi') and func_name in self.content:
                if func_name in DEPRECATED_API:
                    return
                ret = node.type.type.names[0]
                args = []
                for param in node.args.params:
                    quals, type, name = self.get_type(param)
                    param = Param(ptr=self.ptr, array=self.array, qual=quals, type=type, name=name)
                    args.append(param)
                self.function_prototypes[func_name] = (ret, args)


# Parse the header file and extract function prototypes
def extract_function_prototypes(header_code, header):
    parser = c_parser.CParser()  # Set debug parameter to False
    ast = parser.parse(header_code)
    visitor = FunctionVisitor(header)
    visitor.visit(ast)
    return visitor.function_prototypes


def exec_cmd(what, out_file=None):
    p = subprocess.Popen(what, stdin=subprocess.PIPE, stdout=out_file if out_file is not None else subprocess.PIPE, stderr=subprocess.PIPE)
    output_b, err_b = p.communicate()
    rc = p.returncode
    output: str = output_b.decode('utf-8') if output_b is not None else ''
    err: str = err_b.decode('utf-8') if err_b is not None else ''
    return rc, output, err, ' '.join(what)


def preprocess(idf_path, header):
    project_dir = os.path.join(idf_path, 'examples', 'get-started', 'blink')
    build_dir = os.path.join(project_dir, 'build')
    subprocess.check_call(['idf.py', '-B', build_dir, 'reconfigure'], cwd=project_dir)
    build_commands_json = os.path.join(build_dir, 'compile_commands.json')
    with open(build_commands_json, 'r', encoding='utf-8') as f:
        build_command = json.load(f)[0]['command'].split()
    include_dir_flags = []
    include_dirs = []
    # process compilation flags (includes and defines)
    for item in build_command:
        if item.startswith('-I'):
            include_dir_flags.append(item)
            if 'components' in item:
                include_dirs.append(item[2:])  # Removing the leading "-I"
        if item.startswith('-D'):
            include_dir_flags.append(item.replace('\\',''))  # removes escaped quotes, eg: -DMBEDTLS_CONFIG_FILE=\\\"mbedtls/esp_config.h\\\"
    include_dir_flags.append('-I' + os.path.join(build_dir, 'config'))
    temp_file = 'esp_wifi_preprocessed.h'
    with open(temp_file, 'w') as f:
        f.write('#define asm\n')
        f.write('#define volatile\n')
        f.write('#define __asm__\n')
        f.write('#define __volatile__\n')
    with open(temp_file, 'a') as f:
        rc, out, err, cmd = exec_cmd(['xtensa-esp32-elf-gcc', '-w', '-P', '-include', 'ignore_extensions.h', '-E', header] + include_dir_flags, f)
        if rc != 0:
            print(f'command {cmd} failed!')
            print(err)
    preprocessed_code = preprocess_file(temp_file)
    return preprocessed_code


def get_args(parameters):
    params = []
    names = []
    for param in parameters:
        typename = param.type
        if typename == 'void' and param.ptr == 0 and param.name is None:
            params.append(f'{typename}')
            continue
        if param.qual != '':
            typename = f'{param.qual} ' + typename
        declname = param.name
        names.append(f'{declname}')
        if param.ptr > 0:
            declname = '*' * param.ptr + declname
        if param.array > 0:
            declname += f'[{param.array}]'
        params.append(f'{typename} {declname}')
    comma_separated_params = ', '.join(params)
    comma_separated_names = ', '.join(names)
    return comma_separated_params, comma_separated_names


def get_vars(parameters):
    definitions = ''
    names = []
    for param in parameters:
        typename = param.type
        if typename == 'void' and param.ptr == 0 and param.name is None:
            continue
        default_value = '0'
        declname = param.name
        names.append(f'{declname}')
        if param.qual != '':
            typename = f'{param.qual} ' + typename
        if param.ptr > 0:
            declname = '*' * param.ptr + declname
            default_value = 'NULL'
        if param.array > 0:
            declname += f'[{param.array}]'
            default_value = '{}'
        definitions += f'        {typename} {declname} = {default_value};\n'
    comma_separated_names = ', '.join(names)
    return definitions, comma_separated_names


def generate_kconfig_wifi_caps(idf_path, component_path):
    kconfig = os.path.join(component_path, 'Kconfig.soc_wifi_caps.in')
    sdkconfig_files = []
    with open(kconfig, 'w') as out:
        out.write(f'# {AUTO_GENERATED}\n')
        for slave_target in SUPPORTED_TARGETS:
            out.write(f'\nif SLAVE_IDF_TARGET_{slave_target.upper()}\n\n')
            soc_caps = os.path.join(idf_path, 'components', 'soc', slave_target, 'include', 'soc', 'Kconfig.soc_caps.in')
            with open(soc_caps, 'r') as f:
                for line in f:
                    if line.strip().startswith('config SOC_WIFI_'):
                        if 'config SOC_WIFI_SUPPORTED' in line:
                            # if WiFi supported for this target, test it as a slave
                            sdkconfig = os.path.join(component_path, 'test', 'smoke_test', f'sdkconfig.ci.slave_{slave_target}')
                            open(sdkconfig, 'w').write(f'CONFIG_SLAVE_IDF_TARGET_{slave_target.upper()}=y\n')
                            sdkconfig_files.append(sdkconfig)
                        replaced = re.compile(r'SOC_WIFI_').sub('SLAVE_SOC_WIFI_', line)
                        out.write(f'    {replaced}')
                        line = f.readline()         # type
                        out.write(f'    {line}')
                        line = f.readline()         # default
                        out.write(f'    {line}\n')
            out.write(f'endif # {slave_target.upper()}\n')
    return [kconfig] + sdkconfig_files


def generate_test_kconfig(component_path):
    path = os.path.join(component_path, 'test','smoke_test','components','esp_hosted','Kconfig')
    with open(path, 'w') as f:
        f.write(f'# {AUTO_GENERATED}\n')
        f.write('menu "ESP Hosted Mock"\n')
        f.write('    choice SLAVE_IDF_TARGET\n')
        f.write('    prompt "choose slave target"\n')
        f.write('    default SLAVE_IDF_TARGET_ESP32\n')
        for slave_target in SUPPORTED_TARGETS:
            config = 'SLAVE_IDF_TARGET_' + slave_target.upper()
            f.write(f'    config {config}\n')
            f.write(f'        bool "{slave_target}"\n')
        f.write('    endchoice\n')
        f.write('endmenu\n')
    return [path]


def generate_remote_wifi_api(function_prototypes, component_path):
    header = os.path.join(component_path, 'include', 'esp_wifi_remote_api.h')
    wifi_source = os.path.join(component_path, 'esp_wifi_with_remote.c')
    remote_source = os.path.join(component_path, 'esp_wifi_remote_weak.c')
    with open(header, 'w') as f:
        f.write(COPYRIGHT_HEADER)
        f.write('#pragma once\n')
        for func_name, args in function_prototypes.items():
            params, _ = get_args(args[1])
            remote_func_name = NAMESPACE.sub('esp_wifi_remote', func_name)
            f.write(f'{args[0]} {remote_func_name}({params});\n')
    with open(wifi_source, 'w') as wifi, open(remote_source, 'w') as remote:
        wifi.write(COPYRIGHT_HEADER)
        wifi.write('#include "esp_wifi.h"\n')
        wifi.write('#include "esp_wifi_remote.h"\n')
        remote.write(COPYRIGHT_HEADER)
        remote.write('#include "esp_wifi_remote.h"\n')
        remote.write('#include "esp_log.h"\n\n')
        remote.write('#define WEAK __attribute__((weak))\n')
        remote.write('#define LOG_UNSUPPORTED_AND_RETURN(ret) ESP_LOGW("esp_wifi_remote_weak", "%s unsupported", __func__); \\\n         return ret;\n')
        for func_name, args in function_prototypes.items():
            remote_func_name = NAMESPACE.sub('esp_wifi_remote', func_name)
            params, names = get_args(args[1])
            ret_type = args[0]
            ret_value = '-1'     # default return value indicating error
            if (ret_type == 'esp_err_t'):
                ret_value = 'ESP_ERR_NOT_SUPPORTED'
            wifi.write(f'\n{args[0]} {func_name}({params})\n')
            wifi.write('{\n')
            wifi.write(f'    return {remote_func_name}({names});\n')
            wifi.write('}\n')
            remote.write(f'\nWEAK {args[0]} {remote_func_name}({params})\n')
            remote.write('{\n')
            remote.write(f'    LOG_UNSUPPORTED_AND_RETURN({ret_value});\n')
            remote.write('}\n')
    return [header, wifi_source, remote_source]


def generate_hosted_mocks(function_prototypes, component_path):
    source = os.path.join(component_path, 'test', 'smoke_test', 'components', 'esp_hosted', 'esp_hosted_mock.c')
    header = os.path.join(component_path, 'test', 'smoke_test', 'components', 'esp_hosted', 'include', 'esp_hosted_mock.h')
    with open(source, 'w') as f, open(header, 'w') as h:
        f.write(COPYRIGHT_HEADER)
        h.write(COPYRIGHT_HEADER)
        h.write('#pragma once\n')
        f.write('#include "esp_wifi.h"\n')
        f.write('#include "esp_wifi_remote.h"\n')
        for func_name, args in function_prototypes.items():
            hosted_func_name = NAMESPACE.sub('esp_wifi_remote', func_name)
            params, names = get_args(args[1])
            ret_type = args[0]
            ret_value = '0'     # default return value
            if (ret_type == 'esp_err_t'):
                ret_value = 'ESP_OK'
            f.write(f'\n{ret_type} {hosted_func_name}({params})\n')
            f.write('{\n')
            f.write(f'    return {ret_value};\n')
            f.write('}\n')
            h.write(f'{ret_type} {hosted_func_name}({params});\n')
        return [source, header]


def generate_test_cases(function_prototypes, component_path):
    wifi_cases = os.path.join(component_path, 'test', 'smoke_test', 'main', 'all_wifi_calls.c')
    remote_wifi_cases = os.path.join(component_path, 'test', 'smoke_test', 'main', 'all_wifi_remote_calls.c')
    with open(wifi_cases, 'w') as wifi, open(remote_wifi_cases, 'w') as remote:
        wifi.write(COPYRIGHT_HEADER)
        remote.write(COPYRIGHT_HEADER)
        wifi.write('#include "esp_wifi.h"\n\n')
        remote.write('#include "esp_wifi_remote.h"\n\n')
        wifi.write('void run_all_wifi_apis(void)\n{\n')
        remote.write('void run_all_wifi_remote_apis(void)\n{\n')
        for func_name, args in function_prototypes.items():
            remote_func_name = NAMESPACE.sub('esp_wifi_remote', func_name)
            defs, names = get_vars(args[1])
            wifi.write('    {\n')
            wifi.write(f'{defs}')
            wifi.write(f'        {func_name}({names});\n')
            wifi.write('    }\n\n')
            remote.write('    {\n')
            remote.write(f'{defs}')
            remote.write(f'        {remote_func_name}({names});\n')
            remote.write('    }\n\n')
        wifi.write('}\n')
        remote.write('}\n')
    return [wifi_cases, remote_wifi_cases]


def generate_wifi_native(idf_path, component_path):
    wifi_native = os.path.join(component_path, 'include', 'esp_wifi_types_native.h')
    native_header = os.path.join(idf_path, 'components', 'esp_wifi', 'include', 'local', 'esp_wifi_types_native.h')
    orig_content = open(native_header, 'r').read()
    content = orig_content.replace('CONFIG_','CONFIG_SLAVE_')
    open(wifi_native, 'w').write(content)
    return [wifi_native]


def generate_kconfig(idf_path, component_path):
    remote_kconfig = os.path.join(component_path, 'Kconfig')
    slave_configs = ['SOC_WIFI_', 'IDF_TARGET_']
    lines = open(os.path.join(idf_path, 'components', 'esp_wifi', 'Kconfig'), 'r').readlines()
    copy = 100      # just a big number to be greater than nested_if in the first few iterations
    nested_if = 0
    with open(remote_kconfig, 'w') as f:
        f.write(f'# {AUTO_GENERATED}\n')
        f.write('menu "Wi-Fi Remote"\n')
        f.write('    config ESP_WIFI_REMOTE_ENABLED\n')
        f.write('        bool\n')
        f.write('        default y\n\n')
        f.write('    orsource "./Kconfig.soc_wifi_caps.in"\n')
        f.write('    orsource "./Kconfig.rpc.in"\n')
        for line1 in lines:
            line = line1.strip()
            if re.match(r'^if\s+[A-Z_0-9]+\s*$', line):
                nested_if += 1
            elif line.startswith('endif'):
                nested_if -= 1

            if nested_if >= copy:

                for config in slave_configs:
                    line1 = re.compile(config).sub('SLAVE_' + config, line1)
                f.write(line1)

            if re.match(r'^if\s+\(?ESP_WIFI_ENABLED', line):
                copy = nested_if
        f.write('endmenu # Wi-Fi Remote\n')
    return [remote_kconfig]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Build all projects',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog='''\
TEST FAILED
-----------
Some of the component files are different from the pregenerated output.
Please check the files that marked as "FAILED" in this step,
typically you'd just need to commit the changes and create a new version.
Please be aware that the pregenerated files use the same copyright header, so after
making changes you might need to modify 'copyright_header.h' in the script directory.
        ''')
    parser.add_argument('-s', '--skip-check', help='Skip checking the versioned files against the re-generated', action='store_true')
    args = parser.parse_args()

    component_path = os.path.normpath(os.path.join(os.path.realpath(__file__),'..', '..'))
    idf_path = os.getenv('IDF_PATH')
    if idf_path is None:
        raise RuntimeError("Environment variable 'IDF_PATH' wasn't set.")
    header = os.path.join(idf_path, 'components', 'esp_wifi', 'include', 'esp_wifi.h')
    function_prototypes = extract_function_prototypes(preprocess(idf_path, header), header)

    files_to_check = []

    files_to_check += generate_test_kconfig(component_path)

    files_to_check += generate_kconfig_wifi_caps(idf_path, component_path)

    files_to_check += generate_remote_wifi_api(function_prototypes, component_path)

    files_to_check += generate_hosted_mocks(function_prototypes, component_path)

    files_to_check += generate_test_cases(function_prototypes, component_path)

    files_to_check += generate_wifi_native(idf_path, component_path)

    files_to_check += generate_kconfig(idf_path, component_path)

    fail_test = False
    failures = []
    for f in files_to_check:
        print(f'checking {f}')
        rc, out, err, cmd = exec_cmd(['git', 'difftool', '-y', '-x', 'diff -I Copyright', '--', f])
        if out == '' or out.isspace():
            print('    - ok')
        else:
            print('    - FAILED!')
            failures.append((f, out))
            fail_test = True

    if fail_test:
        print(parser.epilog)
        print('\nDIfferent files:\n')
        for i in failures:
            print(f'{i[0]}\nChanges:\n{i[1]}')
        exit(1)
