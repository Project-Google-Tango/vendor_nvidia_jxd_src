// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.util.zip.GZIPOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import android.util.Log;

/**
 * FileUtiles class implements basic file operations such as
 *         getFileSize() - returns folder size,
 *         delete() - deletes files / folders,
 *         copyFile(),
 *         clearFile(),
 *         zipFiles() creates zip file,
 *         createGzip creates a compressed Gzip file.
 */

public class FileUtiles {
    static String TAG = "FileUtiles";

    /**
     * getFileSize() function returns the folder file, in recursive method.
     *
     * @param folder : Folder name whose size needs to be computed.
     * @return foldersize
     */
    public static long getFileSize(File folder) {
        // totalFolder++;
        int totalFile = 0;
        System.out.println("Folder: " + folder.getName());
        long foldersize = 0;

        File[] filelist = folder.listFiles();
        for (int i = 0; i < filelist.length; i++) {
            if (filelist[i].isDirectory()) {
                foldersize += getFileSize(filelist[i]);
            } else {
                totalFile++;
                foldersize += filelist[i].length();
            }
        }
        return foldersize;
    }

    /**
     * getNumberOfFiles() function returns number of time in folder.
     *
     * @param folder : Folder name .
     * @return number of files
     */
    public static long getNumberOfFiles(String folder) {;
        int totalFile = 0;
        File mFile = new File(folder);
        System.out.println("Folder: " + mFile.getName());
        if (mFile.list() != null){
            totalFile = mFile.list().length;
        }else {
            totalFile = 0;
        }

        return totalFile;
    }

    /**
     * delete() function , deletes a file or directory.
     *
     * @param fileName : name of file or folder which needs to be deleted
     * @throws IllegalArgumentException
     */
    public void delete(String fileName) throws IllegalArgumentException {

        File f = new File(fileName);
        if (!f.exists()) throw new IllegalArgumentException("Delete: no such file or directory: " + fileName);
        if (!f.canWrite())    throw new IllegalArgumentException("Delete: write protected: "+ fileName);
        Log.i("Delete", "File to delete is " + fileName);
        if (f.isDirectory()) {
            String[] files = f.list();
            for (int i = 0; i < files.length; i++) {
                delete(f.getAbsolutePath() + "/" + files[i]);
            }
        }
        boolean success = f.delete();
        if (!success) throw new IllegalArgumentException("Delete: deletion failed");
    }

    /**
     * copyFile() copies only files from source to destination.
     *
     * @param source : filename of source file
     * @param destination : file name of destination file.
     */
    public void copyFile(String source, String destination) {
        File inputFile = new File(source);
        if (!inputFile.exists()) return;
        File outputFile = new File(destination);
        FileInputStream in;
        try {
            in = new FileInputStream(inputFile);
            OutputStream out = new FileOutputStream(outputFile);
            byte[] buf = new byte[1024];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            in.close();
            out.close();
        } catch (IOException e) {
            Log.e(TAG, "Unable to copy file");
            e.printStackTrace();
        }
    }

    /**
     * clearFile() is specifically clears the contains of traces.txt.
     *
     * @param fileName File whose contains needs to be deleted.
     * @throws IOException
     */
    public void clearFile(String fileName) {
        try {
            Log.d(TAG, "onStart function clearFile()"+ System.currentTimeMillis());
            File inputFile = new File(fileName);
            if (inputFile.exists()){
                FileWriter out = new FileWriter(inputFile);
                out.write("");
                out.close();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        Log.d(TAG, "onExit function clearFile()" + System.currentTimeMillis());
    }

    /**
     * zipFiles creates a zip file of a directory specified.
     *
     * @param dir Directory to be zipped
     * @param zipFile Output zip file
     * @return true if the operation is successful, else false
     */

    public boolean zipFiles(String dir, String zipFile) {
        Log.d(TAG,"onStart function zipFiles(String dir, String zipFile):" + System.currentTimeMillis());
        try {
            int BUFFER = 2048;
            File dirTree = new File(dir);
            if (!dirTree.isDirectory()) return false;
            File[] files = dirTree.listFiles();
            BufferedInputStream origin = null;
            FileOutputStream dest = new FileOutputStream(zipFile);

            ZipOutputStream out = new ZipOutputStream(new BufferedOutputStream(dest));

            byte data[] = new byte[BUFFER];

            for (int i = 0; i < files.length; i++) {
                Log.i("Zipping","Adding: " + files[i]);
                FileInputStream fi = new FileInputStream(files[i]);
                origin = new BufferedInputStream(fi, BUFFER);
                ZipEntry entry = new ZipEntry(files[i].getName());
                out.putNextEntry(entry);
                int count;
                while ((count = origin.read(data, 0, BUFFER)) != -1) {
                    out.write(data, 0, count);
                }
                origin.close();
            }
            out.close();
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        Log.d(TAG,"onEXit function zipFiles(String dir, String zipFile):" + System.currentTimeMillis());
        return true;
    }

    /**
     * createGzip() compresses a given file into a gzip.
     * @param inputFile file to be compressed.
     * @param gzipFile output gzip file.
     */
    public void createGzip(String inputFile, String gzipFile) {
        Log.d(TAG,"onStart function creategzip():" + System.currentTimeMillis());
        try {
            GZIPOutputStream out = new GZIPOutputStream(new FileOutputStream(gzipFile));
            FileInputStream in = new FileInputStream(inputFile);
            byte[] buf = new byte[1024];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            in.close();
            out.finish();
            out.close();
        } catch (IOException e) {
            Log.i(TAG,"Expection in compressing the file");
            e.printStackTrace();
        }
        Log.d(TAG,"onExit function creategzip():" + System.currentTimeMillis());
    }
}
