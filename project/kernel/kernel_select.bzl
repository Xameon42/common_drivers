load("//common:common_drivers/project/project.bzl", "VENDOR_KERNEL_BUILD")
load("@kernel_toolchain_info//:dict.bzl", "BRANCH")

def kernel_select():
    if VENDOR_KERNEL_BUILD:
        return VENDOR_KERNEL_BUILD
    elif BRANCH == "android14-5.15":
        return "//common:amlogic"
    else:
        return "//common_drivers:amlogic"
