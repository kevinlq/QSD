import qbs
import qbs.Utilities

// no minimum versions are specified, and explicitly set to undefined in
// case the profile has set it
CppApplication {
    files: [qbs.targetOS.contains("darwin") ? "main.mm" : "main.cpp"]
    consoleApplication: true
    cpp.minimumWindowsVersion: undefined
    cpp.minimumMacosVersion: undefined
    cpp.minimumIosVersion: undefined
    cpp.minimumAndroidVersion: undefined

    Properties {
        condition: qbs.targetOS.contains("windows")
        cpp.defines: ["TOOLCHAIN_INSTALL_PATH=" + Utilities.cStringQuote(cpp.toolchainInstallPath)]
    }

    Properties {
        condition: qbs.targetOS.contains("darwin")
        cpp.frameworks: "Foundation"
    }
}
