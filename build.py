#!/usr/bin/python
#
# Modified from TensorFlow installation (https://github.com/tensorflow/tensorflow/blob/ab0fcaceda001825654424bf18e8a8e0f8d39df2/configure.py)
#
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import errno
import os
import platform
import re
import subprocess
import sys

# pylint: disable=g-import-not-at-top
try:
  from shutil import which
except ImportError:
  from distutils.spawn import find_executable as which

_PE_BAZELRC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           '.pe_configure.bazelrc')

def set_build_var(environ_cp, var_name, query_item, option_name,
                  enabled_by_default, bazel_config_name=None):
  """Set if query_item will be enabled for the build.
  Ask user if query_item will be enabled. Default is used if no input is given.
  Set subprocess environment variable and write to .bazelrc if enabled.
  Args:
    environ_cp: copy of the os.environ.
    var_name: string for name of environment variable, e.g. "PE_NEED_MPI".
    query_item: string for feature related to the variable, e.g. "Hadoop File
      System".
    option_name: string for option to define in .bazelrc.
    enabled_by_default: boolean for default behavior.
    bazel_config_name: Name for Bazel --config argument to enable build feature.
  """

  var = str(int(get_var(environ_cp, var_name, query_item, enabled_by_default)))
  environ_cp[var_name] = var
  if var == '1':
    write_to_bazelrc('build --define %s=true' % option_name)
  elif bazel_config_name is not None:
    # TODO(mikecase): Migrate all users of configure.py to use --config Bazel
    # options and not to set build configs through environment variables.
    write_to_bazelrc('build:%s --define %s=true'
                     % (bazel_config_name, option_name))

def write_to_bazelrc(line):
  with open(_PE_BAZELRC, 'a') as f:
    f.write(line + '\n')

def get_var(environ_cp,
            var_name,
            query_item,
            enabled_by_default,
            question=None,
            yes_reply=None,
            no_reply=None):
  """Get boolean input from user.
  If var_name is not set in env, ask user to enable query_item or not. If the
  response is empty, use the default.
  Args:
    environ_cp: copy of the os.environ.
    var_name: string for name of environment variable, e.g. "PE_NEED_MPI".
    query_item: string for feature related to the variable, e.g. "Hadoop File
      System".
    enabled_by_default: boolean for default behavior.
    question: optional string for how to ask for user input.
    yes_reply: optionanl string for reply when feature is enabled.
    no_reply: optional string for reply when feature is disabled.
  Returns:
    boolean value of the variable.
  """
  if not question:
    question = 'Do you wish to build ParallelEnum with %s support?' % query_item
  if not yes_reply:
    yes_reply = '%s support will be enabled for ParallelEnum.' % query_item
  if not no_reply:
    no_reply = 'No %s' % yes_reply

  yes_reply += '\n'
  no_reply += '\n'

  if enabled_by_default:
    question += ' [Y/n]: '
  else:
    question += ' [y/N]: '

  var = environ_cp.get(var_name)
  while var is None:
    user_input_origin = get_input(question)
    user_input = user_input_origin.strip().lower()
    if user_input == 'y':
      print(yes_reply)
      var = True
    elif user_input == 'n':
      print(no_reply)
      var = False
    elif not user_input:
      if enabled_by_default:
        print(yes_reply)
        var = True
      else:
        print(no_reply)
        var = False
    else:
      print('Invalid selection: %s' % user_input_origin)
  return var

def sed_in_place(filename, old, new):
  """Replace old string with new string in file.
  Args:
    filename: string for filename.
    old: string to replace.
    new: new string to replace to.
  """
  with open(filename, 'r') as f:
    filedata = f.read()
  newdata = filedata.replace(old, new)
  with open(filename, 'w') as f:
    f.write(newdata)

def symlink_force(target, link_name):
  """Force symlink, equivalent of 'ln -sf'.
  Args:
    target: items to link to.
    link_name: name of the link.
  """
  try:
    os.symlink(target, link_name)
  except OSError as e:
    if e.errno == errno.EEXIST:
      os.remove(link_name)
      os.symlink(target, link_name)
    else:
      raise e

def get_input(question):
  try:
    try:
      answer = raw_input(question)
    except NameError:
      answer = input(question)  # pylint: disable=bad-builtin
  except EOFError:
    answer = ''
  return answer

def get_from_env_or_user_or_default(environ_cp, var_name, ask_for_var,
                                    var_default):
  """Get var_name either from env, or user or default.
  If var_name has been set as environment variable, use the preset value, else
  ask for user input. If no input is provided, the default is used.
  Args:
    environ_cp: copy of the os.environ.
    var_name: string for name of environment variable, e.g. "PE_NEED_MPI".
    ask_for_var: string for how to ask for user input.
    var_default: default value string.
  Returns:
    string value for var_name
  """
  var = environ_cp.get(var_name)
  if not var:
    var = get_input(ask_for_var)
    print('\n')
  if not var:
    var = var_default
  return var

def set_mpi_home(environ_cp):
  """Set MPI_HOME."""
  default_mpi_home = which('mpirun') or which('mpiexec') or ''
  default_mpi_home = os.path.dirname(os.path.dirname(default_mpi_home))

  ask_mpi_home = ('Please specify the MPI toolkit folder. [Default is %s]: '
                 ) % default_mpi_home
  while True:
    mpi_home = get_from_env_or_user_or_default(environ_cp, 'MPI_HOME',
                                               ask_mpi_home, default_mpi_home)

    if os.path.exists(os.path.join(mpi_home, 'include')) and os.path.exists(
        os.path.join(mpi_home, 'lib')):
      break

    print('Invalid path to the MPI Toolkit. %s or %s cannot be found' %
          (os.path.join(mpi_home, 'include'),
           os.path.exists(os.path.join(mpi_home, 'lib'))))
    environ_cp['MPI_HOME'] = ''

  # Set MPI_HOME
  environ_cp['MPI_HOME'] = str(mpi_home)


def set_other_mpi_vars(environ_cp):
  """Set other MPI related variables."""
  # Link the MPI header files
  mpi_home = environ_cp.get('MPI_HOME')
  symlink_force('%s/include/mpi.h' % mpi_home, 'third_party/mpi/mpi.h')

  # Determine if we use OpenMPI or MVAPICH, these require different header files
  # to be included here to make bazel dependency checker happy
  if os.path.exists(os.path.join(mpi_home, 'include/mpi_portable_platform.h')):
    print('OpenMPI/MVAPICH detected')  
    symlink_force(
        os.path.join(mpi_home, 'include/mpi_portable_platform.h'),
        'third_party/mpi/mpi_portable_platform.h')
    # TODO(gunan): avoid editing files in configure
    sed_in_place('third_party/mpi/mpi.bzl', 'MPI_LIB_IS_OPENMPI=False',
                 'MPI_LIB_IS_OPENMPI=True')
  elif os.path.exists(os.path.join(mpi_home, 'lib/openmpi/include/mpi_portable_platform.h')):
    print('OpenMPI detected')  
    symlink_force(
        os.path.join(mpi_home, 'lib/openmpi/include/mpi_portable_platform.h'),
        'third_party/mpi/mpi_portable_platform.h')
    symlink_force('%s/lib/openmpi/include/mpi.h' % mpi_home, 'third_party/mpi/mpi.h')
    # TODO(gunan): avoid editing files in configure
    sed_in_place('third_party/mpi/mpi.bzl', 'MPI_LIB_IS_OPENMPI=False',
                 'MPI_LIB_IS_OPENMPI=True')
  else:
    # MVAPICH / MPICH
    print('MVAPICH/MPICH detected')  
    symlink_force(
        os.path.join(mpi_home, 'include/mpio.h'), 'third_party/mpi/mpio.h')
    symlink_force(
        os.path.join(mpi_home, 'include/mpicxx.h'), 'third_party/mpi/mpicxx.h')
    # TODO(gunan): avoid editing files in configure
    sed_in_place('third_party/mpi/mpi.bzl', 'MPI_LIB_IS_OPENMPI=True',
                 'MPI_LIB_IS_OPENMPI=False')

  if os.path.exists(os.path.join(mpi_home, 'lib/libmpi.so')):
    symlink_force(
        os.path.join(mpi_home, 'lib/libmpi.so'), 'third_party/mpi/libmpi.so')
  else:
    raise ValueError('Cannot find the MPI library file in %s/lib' % mpi_home)

def remove_line_with(filename, token):
  """Remove lines that contain token from file.
  Args:
    filename: string for filename.
    token: string token to check if to remove a line from file or not.
  """
  with open(filename, 'r') as f:
    filedata = f.read()

  with open(filename, 'w') as f:
    for line in filedata.strip().split('\n'):
      if token not in line:
        f.write(line + '\n')

def reset_pe_configure_bazelrc():
  """Reset file that contains customized config settings."""
  open(_PE_BAZELRC, 'w').close()

  home = os.path.expanduser('~')
  if not os.path.exists('.bazelrc'):
    if os.path.exists(os.path.join(home, '.bazelrc')):
      with open('.bazelrc', 'a') as f:
        f.write('import %s/.bazelrc\n' % home.replace('\\', '/'))
    else:
      open('.bazelrc', 'w').close()

  remove_line_with('.bazelrc', 'pe_configure')
  with open('.bazelrc', 'a') as f:
    f.write('import %workspace%/.pe_configure.bazelrc\n')


def main():
  # Make a copy of os.environ to be clear when functions and getting and setting
  # environment variables.
  environ_cp = dict(os.environ)

  reset_pe_configure_bazelrc()
  set_build_var(environ_cp, 'PE_NEED_MPI', 'MPI', 'with_mpi_support', False)
  if environ_cp.get('PE_NEED_MPI') == '1':
    set_mpi_home(environ_cp)
    set_other_mpi_vars(environ_cp)

if __name__ == '__main__':
  main()
  os.environ['CXX'] = 'mpicxx'
  os.system("bazel build  --cxxopt=\"-O3\" --cxxopt=\"-DOMPI_SKIP_MPICXX\" //ui:text_ui")
  