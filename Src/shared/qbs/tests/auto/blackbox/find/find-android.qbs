import qbs
import qbs.TextFile

Product {
    property string packageName: ""

    Depends { name: "Android.sdk"; required: false }
    Depends { name: "Android.ndk"; required: false }
    type: ["json"]
    Rule {
        multiplex: true
        Artifact {
            filePath: ["android.json"]
            fileTags: ["json"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = output.filePath;
            cmd.sourceCode = function() {
                var tools = {};
                if (product.moduleProperty("Android.sdk", "present")) {
                    tools["sdk"] = product.moduleProperty("Android.sdk", "sdkDir");
                    tools["sdk-build-tools-dx"] = product.Android.sdk.dxFilePath;
                }

                if (product.moduleProperty("Android.ndk", "present")) {
                    tools["ndk"] = product.moduleProperty("Android.ndk", "ndkDir");
                    tools["ndk-samples"] = product.Android.ndk.ndkSamplesDir;
                }
                if (product.java && product.java.present)
                    tools["jar"] = product.java.jarFilePath;

                var tf;
                try {
                    tf = new TextFile(output.filePath, TextFile.WriteOnly);
                    tf.writeLine(JSON.stringify(tools, undefined, 4));
                } finally {
                    if (tf)
                        tf.close();
                }
            };
            return cmd;
        }
    }
}
