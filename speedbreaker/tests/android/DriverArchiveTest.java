// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;
import java.io.*;
import java.nio.file.*;
import java.util.zip.*;
public final class DriverArchiveTest {
    static byte[] elf() { byte[] b=new byte[64]; b[0]=0x7f;b[1]='E';b[2]='L';b[3]='F';b[4]=2;b[5]=1;b[16]=3;b[18]=(byte)183;return b; }
    static byte[] zip(String name,byte[] content) throws Exception {
        ByteArrayOutputStream out=new ByteArrayOutputStream();
        try(ZipOutputStream z=new ZipOutputStream(out)) { z.putNextEntry(new ZipEntry(name));z.write(content);z.closeEntry(); }
        return out.toByteArray();
    }
    static void attempt(String name,byte[] content,boolean accept) throws Exception {
        File folder=Files.createTempDirectory("driver-test").toFile(); boolean passed=true;
        try { DriverArchive.extract(new ByteArrayInputStream(zip(name,content)),folder); } catch(IOException e) { passed=false; }
        finally { File[] files=folder.listFiles(); if(files!=null)for(File f:files)f.delete();folder.delete(); }
        if (passed!=accept) throw new AssertionError(name+": accepted="+passed);
    }
    public static void main(String[] args) throws Exception {
        attempt("libvulkan_freedreno.so",elf(),true);
        attempt("meta.json",new byte[65536],true);
        attempt("meta.json",new byte[65537],false);
        for(String name:new String[]{"../lib.so","/lib.so","sub/lib.so","sub\\lib.so","..","bad.exe"}) attempt(name,elf(),false);
        byte[] wrong=elf();wrong[18]=62;attempt("lib.so",wrong,false);
        wrong=elf();wrong[4]=1;attempt("lib.so",wrong,false);
        wrong=elf();wrong[5]=2;attempt("lib.so",wrong,false);
        wrong=elf();wrong[16]=2;attempt("lib.so",wrong,false);
        attempt("lib.so",new byte[10],false);
        // Entry and per-file limits must use streamed sizes, not ZIP claims.
        File folder=Files.createTempDirectory("driver-count").toFile();
        ByteArrayOutputStream out=new ByteArrayOutputStream();
        try(ZipOutputStream z=new ZipOutputStream(out)) { for(int i=0;i<129;++i){z.putNextEntry(new ZipEntry(i+".txt"));z.closeEntry();} }
        boolean rejected=false;try{DriverArchive.extract(new ByteArrayInputStream(out.toByteArray()),folder);}catch(IOException e){rejected=true;}
        for(File f:folder.listFiles()) f.delete();folder.delete();if(!rejected)throw new AssertionError("entry limit");
        System.out.println("Driver archive path, ELF, metadata and entry boundaries passed");
    }
}
