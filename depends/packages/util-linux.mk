package=util-linux
$(package)_version=2.34
$(package)_download_path=https://mirrors.edge.kernel.org/pub/linux/utils/$(package)/v$($(package)_version)
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=743f9d0c7252b6db246b659c1e1ce0bd45d8d4508b4dfa427bbb4a3e9b9f62b5

define $(package)_set_vars
endef

define $(package)_config_cmds
  ./configure --prefix=$(host_prefix) --disable-switch_root --disable-pivot_root --disable-makeinstall-chown --disable-makeinstall-setuid --disable-use-tty-group --disable-bash-completion
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
