include($$PWD/../QSD.pri)

# Config Para
CONFIG(debug, debug|release):{
        FILE_POSTFIX = D
        DIR_COMPILEMODE = Debug
}
else:CONFIG(release, debug|release):{
        FILE_POSTFIX =
        DIR_COMPILEMODE = Release
}

win32:{
        CONFIG(MinGW, MinGW|MSVC):{
                DIR_COMPILER = MinGW
                FILE_LIB_PREFIX = lib
                FILE_LIB_EXT = .a
        }
        else:CONFIG(MSVC, MinGW|MSVC):{
                DIR_COMPILER = MSVC
                FILE_LIB_PREFIX =
                FILE_LIB_EXT = .lib
        }

        DIR_PLATFORM = Win32
        FILE_DLL_PREFIX =
        FILE_DLL_EXT = .dll

        DESTDIR  = $$PWD/../Bin/$${DIR_PLATFORM}/$${DIR_COMPILER}/$${DIR_COMPILEMODE}/$${IDE_CASED_ID}/lib/$${IDE_ID}/plugins
}
else:android:{
        CONFIG(ARM_GCC_4.4.3, ARM_GCC_4.4.3|ARM_GCC_4.6|ARM_GCC_4.7|ARM_GCC_4.8):{
                DIR_COMPILER = ARM_GCC_4.4.3
        }
        else:CONFIG(ARM_GCC_4.6, ARM_GCC_4.4.3|ARM_GCC_4.6|ARM_GCC_4.7|ARM_GCC_4.8):{
                DIR_COMPILER = ARM_GCC_4.6
        }
        else:CONFIG(ARM_GCC_4.7, ARM_GCC_4.4.3|ARM_GCC_4.6|ARM_GCC_4.7|ARM_GCC_4.8):{
                DIR_COMPILER = ARM_GCC_4.7
        }
        else:CONFIG(ARM_GCC_4.8, ARM_GCC_4.4.3|ARM_GCC_4.6|ARM_GCC_4.7|ARM_GCC_4.8):{
                DIR_COMPILER = ARM_GCC_4.8
        }

        CONFIG(X86_GCC_4.4.3, X86_GCC_4.4.3|X86_GCC_4.6|X86_GCC_4.7|X86_GCC_4.8):{
                DIR_COMPILER = X86_GCC_4.4.3
        }
        else:CONFIG(X86_GCC_4.6, X86_GCC_4.4.3|X86_GCC_4.6|X86_GCC_4.7|X86_GCC_4.8):{
                DIR_COMPILER = X86_GCC_4.6
        }
        else:CONFIG(X86_GCC_4.7, X86_GCC_4.4.3|X86_GCC_4.6|X86_GCC_4.7|X86_GCC_4.8):{
                DIR_COMPILER = X86_GCC_4.7
        }
        else:CONFIG(X86_GCC_4.8, X86_GCC_4.4.3|X86_GCC_4.6|X86_GCC_4.7|X86_GCC_4.8):{
                DIR_COMPILER = X86_GCC_4.8
        }

        DIR_PLATFORM = Android
        FILE_LIB_PREFIX = lib
        FILE_LIB_EXT = .a
        FILE_DLL_PREFIX = lib
        FILE_DLL_EXT = .so

        # 始终编译成静态库
        CONFIG += staticlib
        CONFIG += USE_LIBRARY_ABN

        DESTDIR  = $$PWD/../Bin/$${DIR_PLATFORM}/$${DIR_COMPILER}/$${DIR_COMPILEMODE}/$${IDE_CASED_ID}/lib/$${IDE_ID}/plugins
}
else:ios:{
        CONFIG(LLVM, LLVM):{
                DIR_COMPILER = LLVM
        }

        DEFINES += IOS
        DIR_PLATFORM = IOS
        FILE_LIB_PREFIX = lib
        FILE_LIB_EXT = .a
        FILE_DLL_PREFIX = lib
        FILE_DLL_EXT = .so

        # 始终编译成静态库
        CONFIG += staticlib
        CONFIG += USE_LIBRARY_ABN

        DESTDIR  = $$PWD/../Bin/$${DIR_PLATFORM}/$${DIR_COMPILER}/$${DIR_COMPILEMODE}/$${IDE_CASED_ID}/lib/$${IDE_ID}/plugins
}
else:mac:{
        CONFIG(clang, clang):{
                DIR_COMPILER = clang
        }

        DEFINES += MAC
        DIR_PLATFORM = MAC
        FILE_LIB_PREFIX = lib
        FILE_LIB_EXT = .a
        FILE_DLL_PREFIX = lib
        FILE_DLL_EXT = .so

        # 始终编译成静态库
        CONFIG += staticlib
        CONFIG += USE_LIBRARY_ABN

        DESTDIR  = $$PWD/../Bin/$${DIR_PLATFORM}/$${DIR_COMPILER}/$${DIR_COMPILEMODE}/$${IDE_CASED_ID}/lib/$${IDE_ID}/plugins
}
else:linux:{
        CONFIG(GCC, GCC|GCC32|GCC64):{
                        DIR_COMPILER = GCC32
                        DIR_PLATFORM = Linux32
        }
        else:CONFIG(GCC32, GCC|GCC32|GCC64):{
                        DIR_COMPILER = GCC32
                        DIR_PLATFORM = Linux32
        }
        else:CONFIG(GCC64, GCC|GCC32|GCC64):{
                        DIR_COMPILER = GCC64
                        DIR_PLATFORM = Linux64
        }

        FILE_LIB_PREFIX = lib
        FILE_LIB_EXT = .a
        FILE_DLL_PREFIX = lib
        FILE_DLL_EXT = .so

        DESTDIR  = $$PWD/../Bin/$${DIR_PLATFORM}/$${DIR_COMPILER}/$${DIR_COMPILEMODE}/$${IDE_CASED_ID}/lib/$${IDE_ID}/plugins
}

DIR_DEPEND_DEST = $$PWD/../Bin/$${DIR_PLATFORM}/$${DIR_COMPILEMODE}/$${IDE_CASED_ID}/bin

# add for plugin
depfile = $$replace(_PRO_FILE_PWD_, ([^/]+$), \\1/\\1_dependencies.pri)
exists($$depfile) {
    include($$depfile)
    isEmpty(QTC_PLUGIN_NAME): \
        error("$$basename(depfile) does not define QTC_PLUGIN_NAME.")
} else {
    isEmpty(QTC_PLUGIN_NAME): \
        error("QTC_PLUGIN_NAME is empty. Maybe you meant to create $$basename(depfile)?")
}
TARGET = $$QTC_PLUGIN_NAME

#plugin_deps = $$QTC_PLUGIN_DEPENDS
#plugin_test_deps = $$QTC_TEST_DEPENDS
#plugin_recmds = $$QTC_PLUGIN_RECOMMENDS


# use gui precompiled header for plugins by default
isEmpty(PRECOMPILED_HEADER):PRECOMPILED_HEADER = $$PWD/shared/qtcreator_gui_pch.h

PLUGINJSON = $$_PRO_FILE_PWD_/$${TARGET}.json
PLUGINJSON_IN = $${PLUGINJSON}.in
exists($$PLUGINJSON_IN) {
    DISTFILES += $$PLUGINJSON_IN
    QMAKE_SUBSTITUTES += $$PLUGINJSON_IN
    PLUGINJSON = $$OUT_PWD/$${TARGET}.json
} else {
    # need to support that for external plugins
    DISTFILES += $$PLUGINJSON
}
