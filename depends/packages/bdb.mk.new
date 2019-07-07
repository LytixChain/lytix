package=bdb
$(package)_version=18.1.32
$(package)_download_path=http://www.lytixchain.org/depends
$(package)_file_name=db-$($(package)_version).tar.gz
$(package)_sha256_hash=fa1fe7de9ba91ad472c25d026f931802597c29f28ae951960685cde487c8d654
$(package)_build_subdir=build_unix

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-cxx --disable-replication
$(package)_config_opts_mingw32=--enable-mingw
$(package)_config_opts_linux=--with-pic
$(package)_cxxflags=-std=c++11
endef

define $(package)_preprocess_cmds
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub dist
endef

define $(package)_config_cmds
  ../dist/$($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) libdb_cxx-18.1.a libdb-18.1.a
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install_lib install_include
endef
