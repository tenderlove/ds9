require 'mkmf'

def using_system_libraries?
  arg_config('--use-system-libraries', !!ENV['DS9_USE_SYSTEM_LIBRARIES'])
end

dir_config('ds9')

if using_system_libraries?
  if with_config("static")
    ldflags = pkg_config 'libnghttp2', 'libs-only-L'

    archive = File.join ldflags.sub(/^-L/, ''), 'libnghttp2.a'
    if File.exist? archive
      $LDFLAGS << " #{archive}"
    else
      raise "couldn't find archive"
    end
  else
    pkg_config 'libnghttp2'
  end
else
  message "Building nghttp2\n"
  require 'rubygems'
  require 'mini_portile2'
  recipe = MiniPortile.new('nghttp2', 'v1.71.0')
  recipe.configure_options = recipe.configure_options + ['--with-pic', '--enable-lib-only']

  recipe.files << {
    url: 'https://github.com/nghttp2/nghttp2/releases/download/v1.71.0/nghttp2-1.71.0.tar.gz',
    sha256: '0cdd35ecec77919b4b7b245f87fe8d2216e3d62da854eb0838c594ff058042fc',
  }
  recipe.cook

  # `recipe.activate` uses invalid path for this package.
  $LIBPATH = ["#{recipe.path}/lib"] + $LIBPATH
  $CPPFLAGS << " -I#{recipe.path}/include"
  $LIBS << " -lstdc++"
end

abort 'nghttp2/nghttp2.h not found' unless have_header('nghttp2/nghttp2.h')
abort 'libnghttp2 not found' unless have_library('nghttp2')

have_func('rb_io_descriptor', 'ruby/io.h')

create_makefile 'ds9'
