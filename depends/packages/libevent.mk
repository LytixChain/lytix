package=libevent
$(package)_version=2.1.10-stable
$(package)_download_path=https://github.com/libevent/libevent/releases/
$(package)_file_name=release-$($(package)_version).tar.gz
$(package)_sha256_hash=e864af41a336bb11dab1a23f32993afe963c1f69618bd9292b89ecf6904845b0

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
