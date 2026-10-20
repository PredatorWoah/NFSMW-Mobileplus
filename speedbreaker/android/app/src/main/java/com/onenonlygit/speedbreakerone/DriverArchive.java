// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;
import java.io.*;
import java.util.*;
import java.util.zip.*;
/** Flat driver bundle extraction, shared with the host boundary tests. */
final class DriverArchive {
    static void extract(InputStream source, File folder) throws IOException {
        if (source == null) throw new IOException("Cannot read this document.");
        Set<String> names = new HashSet<>(); long total=0; int entries=0;
        try (ZipInputStream zip=new ZipInputStream(source)) {
            ZipEntry entry; byte[] buffer=new byte[65536];
            while ((entry=zip.getNextEntry())!=null) {
                if (++entries>128) throw new IOException("Too many ZIP entries.");
                String name=entry.getName();
                // The ZIP's file type is ignored: output files are newly created
                // regular files, so a ZIP symlink can never escape this folder.
                if (entry.isDirectory()) {
                    if (name.startsWith("/") || name.contains("..") || name.contains("\\")) throw new IOException("Unsafe ZIP path.");
                    continue;
                }
                if (!name.matches("[A-Za-z0-9_+.-]+") || name.contains("..") || !names.add(name)) throw new IOException("Unsafe or duplicate ZIP entry.");
                if (!(name.equals("meta.json") || name.endsWith(".so") || name.toLowerCase(Locale.ROOT).endsWith(".txt") || name.toLowerCase(Locale.ROOT).endsWith(".md") || name.startsWith("LICENSE"))) throw new IOException("Unsupported file in driver ZIP: "+name);
                long size=0;
                try (OutputStream out=new FileOutputStream(new File(folder,name))) {
                    int n; while ((n=zip.read(buffer))!=-1) { size+=n; total+=n;
                        if (size>128L*1024*1024 || total>256L*1024*1024 || (name.equals("meta.json") && size>65536)) throw new IOException("Driver ZIP exceeds size limit.");
                        out.write(buffer,0,n);
                    }
                }
                if (name.endsWith(".so")) validateElf(new File(folder,name));
            }
        }
    }
    static void validateElf(File file) throws IOException {
        byte[] h=new byte[64]; try (DataInputStream in=new DataInputStream(new FileInputStream(file))) { in.readFully(h); }
        if (h[0]!=0x7f || h[1]!='E' || h[2]!='L' || h[3]!='F' || h[4]!=2 || h[5]!=1 || h[16]!=3 || h[17]!=0 || (h[18]&255)!=183 || h[19]!=0)
            throw new IOException("Driver must contain ARM64 little-endian shared libraries.");
    }
}
