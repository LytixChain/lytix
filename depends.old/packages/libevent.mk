package=libevent
$(package)_version=2.1.10-stable
$(package)_download_path=https://github.com/libevent/libevent/archive/
$(package)_file_name=release-$($(package)_version).tar.gz
$(package)_sha256_hash=52c9db0bc5b148f146192aa517db0762b2a5b3060ccc63b2c470982ec72b9a79

define $(package)_preprocess_cmds
  ./autogen.sh
endef

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --disable-openssl --disable-libevent-regress --disable-samples
  $(package)_config_opts_release=--disable-debug-mode
  $(package)_config_opts_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
