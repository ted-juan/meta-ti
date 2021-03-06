# This .inc file allows building and deploying multiple sets of kernel + modules
# with different defconfigs
#
# Note that this include will NOT stage anything nor create packages, since that
# is virtually impossible. Userspace should be built against the 'regular' kernel
#
# The intended usecase is for machines that have mutually exclusive drivers due
# to e.g. pinmuxing issues. For example the LogicPD omap-l138 experimenter board
# can have multiple mutually exclusive expansion boards like lcd, ethernet,
# sound, 20x2 character LCD, etc. To be able to easily test all of those you can
# use this .inc
#
# To make it easier finding the original defconfig from a running kernel, this
# also forcefully turns on CONFIG_IKCONFIG_PROC so people can do
# 'zcat /proc/config.gz' on the target.
#
# The function get_kernelversion uses generated header files to extract the
# version number. But those files are not available until do_compile, and
# do_compileconfigs is injected before that, hence the version becomes None
# and breaks in several places. Introduce a task do_preparekernel that calls
# "make prepare" in the kernel tree to generate all the necessary files.

require linux.inc

SRC_URI += " \
           file://configs "

MULTI_CONFIG_BASE_SUFFIX = "multi-config-"
MODULE_IMAGE_BASE_NAME ?= "modules-${PE}-${PV}-${PR}-${MACHINE}-${DATETIME}"

do_preparekernel () {
	unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS MACHINE
	oe_runmake prepare CC="${KERNEL_CC}" LD="${KERNEL_LD}"
}

addtask preparekernel after do_configure before do_compile

do_compileconfigs () {

  # fixup for fetcher change
  if [ -e ${WORKDIR}/configs/configs ] ; then
      if [ -e ${WORKDIR}/configs/configs/empty ] ; then
          mv ${WORKDIR}/configs/configs/empty ${WORKDIR}/configs/
      fi
      mv ${WORKDIR}/configs/configs/* ${WORKDIR}/configs/
      rm -rf ${WORKDIR}/configs/configs
  fi

  # Compile and Install additional kernel configs if found
  if [ -e ${WORKDIR}/configs/empty ] ; then
       echo "No configs found in configs/ directory, skipping to regular build"
  else
      echo "Multiple configs found, building those first"

      # Context Save the regular 'defconfig'
      cp ${WORKDIR}/defconfig ${WORKDIR}/defconfig.save

      for config in ${WORKDIR}/configs/* ; do

        # Copy in alternative config
        cd ${S}
        cp $config ${WORKDIR}/defconfig

        # Enable config to be viewed on the target
        echo "CONFIG_IKCONFIG=y" >> ${WORKDIR}/defconfig
        echo "CONFIG_IKCONFIG_PROC=y" >> ${WORKDIR}/defconfig
	
        # Build and Install this alternative kernel
        do_configure
        kernel_do_compile
        do_compile_kernelmodules
        kernel_do_install

        # Drop the resulting images in the deploy dir
        install -d ${DEPLOY_DIR_IMAGE}
        install -m 0644 ${KERNEL_OUTPUT} ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGE_BASE_NAME}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).bin

        if [ -d "${D}/lib" ]; then
            tar --owner=root --group=root -cvzf ${DEPLOY_DIR_IMAGE}/${MODULE_IMAGE_BASE_NAME}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).tgz -C ${D} lib
        fi
 
        # Install the final config alongside the images
        cp .config ${DEPLOY_DIR_IMAGE}/config-${PV}-${PR}-${MACHINE}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).config

        # Create symlinks
        cd ${DEPLOY_DIR_IMAGE}
        rm -f ${KERNEL_IMAGE_SYMLINK_NAME}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).bin
        ln -sf ${KERNEL_IMAGE_BASE_NAME}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).bin ${KERNEL_IMAGE_SYMLINK_NAME}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).bin
        rm -f modules-${MACHINE}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).tgz
        ln -sf ${MODULE_IMAGE_BASE_NAME}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).tgz modules-${MACHINE}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).tgz
        rm -f config-${MACHINE}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).config
        ln -sf config-${PV}-${PR}-${MACHINE}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).config config-${MACHINE}.${MULTI_CONFIG_BASE_SUFFIX}$(basename $config).config

      done

      # Restore the regular 'defconfig'
      cp ${WORKDIR}/defconfig.save ${WORKDIR}/defconfig
      cd ${S}
      do_configure
    fi
}

# For reference, copy .config to deploy image
do_deploy_append () {

	install -d ${DEPLOY_DIR_IMAGE}

    # Drop the regular defconfig along side the others for consistency
    cd ${S}
    cp .config ${DEPLOY_DIR_IMAGE}/config-${PV}-${PR}-${MACHINE}.config

    # add symlink
    cd ${DEPLOY_DIR_IMAGE}    
    rm -f config-${MACHINE}.config
    ln -s config-${PV}-${PR}-${MACHINE}.config config-${MACHINE}.config

    rm -f modules-${MACHINE}.tgz
    ln -sf ${MODULE_IMAGE_BASE_NAME}.tgz modules-${MACHINE}.tgz

}

do_compileconfigs[depends] += "u-boot-mkimage-native:do_populate_sysroot virtual/${TARGET_PREFIX}gcc:do_populate_sysroot"
addtask compileconfigs after do_preparekernel before do_compile
