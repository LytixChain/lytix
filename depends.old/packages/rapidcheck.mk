package=rapidcheck
$(package)_version=b2032e6
$(package)_download_path=https://github.com/MarcoFalke/rapidcheck/archive
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=e88b9b845715ecac003050f47c63d331cf93be86b2d7720629ab3ead6a702d1d

define $(package)_config_cmds
  cmake -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=true .
endef

define $(package)_build_cmds
  $(MAKE) && \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a include/* $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp -a extras/boost_test/include/rapidcheck/* $($(package)_staging_dir)$(host_prefix)/include/rapidcheck/ && \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/lib && \
  cp -a librapidcheck.a $($(package)_staging_dir)$(host_prefix)/lib/
endef
