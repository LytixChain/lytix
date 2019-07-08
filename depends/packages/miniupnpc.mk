package=miniupnpc
$(package)_version=2.1.20190408
$(package)_download_path=https://www.lytixchain.org/depends
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=a0c46bcf6065d6351a8fa6a0a18dc57d10a16908dbb470908fd2e423511514ec

define $(package)_set_vars
$(package)_build_opts=CC="$($(package)_cc)"
$(package)_build_opts_darwin=OS=Darwin LIBTOOL="$($(package)_libtool)"
$(package)_build_opts_mingw32=-f Makefile.mingw
$(package)_build_env+=CFLAGS="$($(package)_cflags) $($(package)_cppflags)" AR="$($(package)_ar)"
endef

define $(package)_preprocess_cmds
  mkdir dll && \
  sed -e 's|MINIUPNPC_VERSION_STRING \"version\"|MINIUPNPC_VERSION_STRING \"$($(package)_version)\"|' -e 's|OS/version|$(host)|' miniupnpcstrings.h.in > miniupnpcstrings.h && \
  sed -i.old "s|miniupnpcstrings.h: miniupnpcstrings.h.in wingenminiupnpcstrings|miniupnpcstrings.h: miniupnpcstrings.h.in|" Makefile.mingw
endef

define $(package)_build_cmds
	$(MAKE) libminiupnpc.a $($(package)_build_opts)
endef

define $(package)_stage_cmds
	mkdir -p $($(package)_staging_prefix_dir)/include/miniupnpc $($(package)_staging_prefix_dir)/lib &&\
	install *.h $($(package)_staging_prefix_dir)/include/miniupnpc &&\
	install libminiupnpc.a $($(package)_staging_prefix_dir)/lib
endef
