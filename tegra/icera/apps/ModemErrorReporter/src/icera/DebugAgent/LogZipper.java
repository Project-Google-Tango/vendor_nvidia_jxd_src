// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;

public class LogZipper {
    public static void zipFile(String srcFileName, String zipFileName) throws Exception {
        ZipOutputStream outZip = new ZipOutputStream(new FileOutputStream(zipFileName));
        File inFile = new File(srcFileName);

        if (inFile.isFile()) {
            byte[] buf = new byte[4096];
            int len;
            ZipEntry zipEntry = new ZipEntry(inFile.getName());
            FileInputStream inputStream = new FileInputStream(inFile);
            outZip.putNextEntry(zipEntry);

            while ((len = inputStream.read(buf)) != -1) {
                outZip.write(buf, 0, len);
            }
            outZip.closeEntry();
            inputStream.close();
        }
        outZip.finish();
        outZip.close();
    }
}
