project('libdwarf', 'c',
  version: '20201201',
  default_options: [
    'c_std=gnu99',
  ],
)

v_maj = 1
v_min = 0
v_mic = 0

lt_cur = v_maj + v_min
lt_rev = v_mic
lt_age = v_min

host_os = host_machine.system()
if host_os in ['hpux', 'netbsd', 'openbsd', 'sunos']
  library_version = '@0@.@1@'.format(lt_cur, lt_rev)
elif host_os == 'qnx'
  library_version = lt_cur.to_string()
else
  library_version = '@0@.@1@.@2@'.format(lt_cur - lt_age, lt_age, lt_rev)
endif
darwin_versions = [lt_cur + 1, '@0@.@1@'.format(lt_cur + 1, lt_rev)]

cc = meson.get_compiler('c')

if meson.is_subproject() and cc.get_argument_syntax() != 'msvc'
  add_project_arguments('-w', language: 'c')
endif

libelf_dep = dependency('libelf', required: false)
if not libelf_dep.found()
  if cc.has_header('libelf.h')
    libelf_dep = cc.find_library('elf')
  else
    libelf_dep = dependency('libelf', required: true)
  endif
endif
zlib_dep = dependency('zlib', required: false)

cdata = configuration_data()

project_name = meson.project_name()
project_version = meson.project_version()
cdata.set_quoted('PACKAGE', project_name)
cdata.set_quoted('PACKAGE_NAME', project_name)
cdata.set_quoted('PACKAGE_TARNAME', project_name)
cdata.set_quoted('PACKAGE_VERSION', project_version)
cdata.set_quoted('VERSION', project_version)
cdata.set_quoted('PACKAGE_STRING', project_name + ' ' + project_version)
cdata.set_quoted('PACKAGE_URL', '')
cdata.set_quoted('PACKAGE_BUGREPORT', 'libdwarf-list -at- linuxmail -dot- org')

# No need to detect this for the platforms we currently support.
cdata.set('HAVE_ALLOCA', 1)

if cc.compiles('''static unsigned int foo(unsigned int x, __attribute__ ((unused)) int y) {
                      unsigned int x2 = x + 1;
                      return x2;
                  }
                  int goo() {
                      unsigned int y = 0;
                      y = foo(12, y);
                  }''',
               args: ['-Werror'],
               name: 'compiler supports __attribute__ ((unused))')
  cdata.set('HAVE_UNUSED_ATTRIBUTE', 1)
endif

cdata.set('STDC_HEADERS', 1)

system_headers = [
  'alloca.h',
  'dlfcn.h',
  'inttypes.h',
  'malloc.h',
  'memory.h',
  'regex.h',
  'stddef.h',
  'stdint.h',
  'stdlib.h',
  'string.h',
  'strings.h',
  'sys/stat.h',
  'sys/types.h',
  'unistd.h',
  'windows.h',
]
foreach h : system_headers
  if cc.has_header(h)
    cdata.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

if cdata.has('HAVE_STDINT_H')
  if cc.compiles('''#include <stdint.h>
                    int foo(void) {
                        uintptr_t i = 42;
                        return (int)i;
                    }''',
                 name: 'uintptr_t defined in stdint.h')
    cdata.set('HAVE_UINTPTR_T', 1)
  endif
  if cc.compiles('''#include <stdint.h>
                    int foo(void) {
                        intptr_t i = 42;
                        return (int)i;
                    }''',
                 name: 'intptr_t defined in stdint.h')
    cdata.set('HAVE_INTPTR_T', 1)
  endif
endif

if cc.compiles('''#if @0@
                  # include <sys/types.h>
                  #endif
                  #if @1@
                  # include <regex.h>
                  #endif

                  void foo(void) {
                      int i;
                      regex_t r;
                      int cflags = REG_EXTENDED;
                      const char *s = "abc";
                      i = regcomp(&r,s,cflags);
                      regfree(&r);
                  }
                  '''.format(cdata.has('HAVE_SYS_TYPES_H').to_int(),
                             cdata.has('HAVE_REGEX_H').to_int()),
               name: 'regex library is available')
  cdata.set('HAVE_REGEX', 1)
endif

cdata.set('DWARF_WITH_LIBELF', 1)
if libelf_dep.type_name() == 'internal'
  cdata.set('HAVE_ELF_H', 1)
  cdata.set('HAVE_LIBELF_H', 1)
  cdata.set('HAVE_ELF64_GETEHDR', 1)
  cdata.set('HAVE_ELF64_GETSHDR', 1)
  cdata.set('HAVE_ELF64_RELA', 1)
  cdata.set('HAVE_ELF64_SYM', 1)
  cdata.set('HAVE_ELF64_R_INFO', 1)
else
  if cc.has_header('elf.h', dependencies: [libelf_dep])
    cdata.set('HAVE_ELF_H', 1)
  endif
  if cc.has_header('libelf.h', dependencies: [libelf_dep])
    cdata.set('HAVE_LIBELF_H', 1)
  endif
  if cc.has_function('elf64_getehdr', dependencies: [libelf_dep])
    cdata.set('HAVE_ELF64_GETEHDR', 1)
  endif
  if cc.has_function('elf64_getshdr', dependencies: [libelf_dep])
    cdata.set('HAVE_ELF64_GETSHDR', 1)
  endif
  if cc.has_header_symbol('elf.h', 'Elf64_Rela', dependencies: [libelf_dep])
    cdata.set('HAVE_ELF64_RELA', 1)
  endif
  if cc.has_header_symbol('elf.h', 'Elf64_Sym', dependencies: [libelf_dep])
    cdata.set('HAVE_ELF64_SYM', 1)
  endif
  if cc.has_member('Elf64_Rel', 'r_info', dependencies: [libelf_dep], prefix: '#include <elf.h>')
    cdata.set('HAVE_ELF64_R_INFO', 1)
  endif
endif

if zlib_dep.found()
  cdata.set('HAVE_ZLIB', 1)
  cdata.set('HAVE_ZLIB_H', 1)
endif

configure_file(
  input: 'config.h.in.meson',
  output: 'config.h',
  configuration: cdata,
)

libdwarf_internal_incdirs = include_directories(
  '.',
)

fix_libdwarf_elf = files('scripts' / 'fixlibdwarfelf.py')

subdir('libdwarf')
