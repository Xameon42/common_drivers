#!/bin/bash

enter_path=$(pwd)
auto_patch_path=$(dirname $(readlink -f "$0"))
common_driver_path=$(dirname $(dirname $0))
COMMON_DRIVERS_DIR=${common_driver_path##*/}
common_kernel_path=$(readlink -f $(dirname $common_driver_path))
KERNEL_DIR=${common_kernel_path##*/}
common_path=$(dirname $common_kernel_path)
cd ${common_path}
if [[ $# < 1 ]]; then
	version_message=$(grep -rn BRANCH= ${common_kernel_path}/build.config.constants)
	version_message="common${version_message##*android}"
	FULL_KERNEL_VERSION=${version_message}
else
	FULL_KERNEL_VERSION=$1
fi
PATCHES_PATH=${auto_patch_path}/${FULL_KERNEL_VERSION}

if [[ ! -d ${PATCHES_PATH} ]]; then
	echo "None patch to am, ${PATCHES_PATH}/${FULL_KERNEL_VERSION}/patches does not exist!!!"
	cd ${enter_path}
	exit
fi

# echo KERNEL_DIR=$KERNEL_DIR COMMON_DRIVERS_DIR=$COMMON_DRIVERS_DIR enter_path=$enter_path PATCHES_PATH=$PATCHES_PATH

function am_patch()
{
	local patch=$1
	local dir=$2
	local compare_id=$3
	local change_id=`grep 'Change-Id' $patch | head -n1 | awk '{print $2}'`

	# echo ${patch} ${dir}
	if [ -d "${dir}" ]; then
		cd ${dir};
		if [[ ! "${compare_id[@]}" =~ "${change_id}" ]]; then
			# echo "###patch ${patch##*/}###      "
			git am -q ${patch} 1>/dev/null 2>&1;
			if [ $? != 0 ]; then
				git am --abort
				cd ${enter_path}
				echo "Patch Error : Failed to patch [$patch], Need check it. exit!!!"
				exit -1
			fi
			echo -n .
		fi
		cd ${common_path}
	fi
}

function auto_patch()
{
	local patch_dir=$1
	# echo patch_dir=$patch_dir

	for file in `find ${patch_dir} -name "*.patch" | sort`; do
		local file_name=${file%.*};           #echo file_name $file_name
		local resFile=`basename $file_name`;  #echo resFile $resFile
		local dir_name1=${resFile//#/\/};     #echo dir_name $dir_name
		local dir_name=${dir_name1%/*};       #echo dir_name $dir_name
		local dir=${common_path}/$dir_name;   #echo $dir
		[[ ! -d ${dir}/.git ]] && continue
		local compare_change_id=$(cd ${dir} && git log -n 400 |grep "Change-Id:" | awk '{print $2}')
		am_patch ${file} ${dir} "${compare_change_id[@]}"
	done
}

function traverse_patch_dir()
{
	# git am common and common_driver patches
	echo "Auto Patch Start"
	{
		local common_change_id=$(cd ${KERNEL_DIR} && git log -n 400 |grep "Change-Id:" | awk '{print $2}')
		for file in `ls ${PATCHES_PATH}/common`; do
			# echo file=$file
			if [ -d ${PATCHES_PATH}/common/${file} ]; then
				for patch in `find ${PATCHES_PATH}/common/${file} -name "*.patch" | sort`; do
					am_patch ${patch} ${KERNEL_DIR} "${common_change_id[@]}"
				done
			fi
		done
	} &

	{
		local common_drivers_change_id=$(cd ${KERNEL_DIR}/${COMMON_DRIVERS_DIR} && git log -n 400 |grep "Change-Id:" | awk '{print $2}')
		if [[ -d ${PATCHES_PATH}/common_drivers ]]; then
			for patch in `find ${PATCHES_PATH}/common_drivers -name "*.patch" | sort`; do
				am_patch ${patch} ${KERNEL_DIR}/${COMMON_DRIVERS_DIR} "${common_drivers_change_id[@]}"
			done
		fi
	} &

	for file in `ls ${PATCHES_PATH}`; do
		[[ "${file}" == "common" || "${file}" == "common_drivers" ]] && continue

		if [ -d ${PATCHES_PATH}/${file} ]; then
			local dest_dir=${PATCHES_PATH}/${file}
			auto_patch ${dest_dir}
		fi
	done

	wait
	echo
	echo "Patch Finish: ${common_path}"
}

function handle_lunch_patch()
{
	echo "Auto Patch lunch's patch Start"

	while read line_patch; do
		local patch_dir=${line_patch%/*}
		local patch_file=${line_patch##*/}
		local file=${PATCHES_PATH}/${patch_dir}/${patch_file}

		local file_name=${file%.*}
		local resFile=`basename $file_name`
		local dir_name1=${resFile//#/\/}
		local dir_name=${dir_name1%/*}
		local dir=${common_path}/$dir_name

		local compare_change_id=$(cd ${dir} && git log -n 400 |grep "Change-Id:" | awk '{print $2}')
		am_patch ${file} ${dir} ${compare_change_id}
	done < ${PATCHES_PATH}/lunch_patches.txt

	echo
	echo "Patch Finish: ${common_path}"
}

if [[ "${PATCH_PARM}" == "lunch" ]]; then
	handle_lunch_patch
else
	traverse_patch_dir
fi
cd ${enter_path}
