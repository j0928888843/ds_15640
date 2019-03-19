/**
 * Proxy which supports open, read, write, unlink, and lseek RPC from client
 * Use java RMI to connect to server
 * The server file can be cached at proxy with LRU cache.
 * the file semantics is open close semantixs and the check-on-use protocol.
 *
 * Use fb_lock and cache_lock to lock the cache access operation and fd generation 
 * ,to ensure atomic under cocurrent condition
 */
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.io.*;
import java.nio.file.*;
import java.rmi.registry.Registry;
import java.rmi.registry.LocateRegistry;
import java.rmi.server.UnicastRemoteObject;
import java.rmi.*;

class Proxy {
    private static final int MAX_FILENUM = 99999999;       // Max numer of file that can open
    private final static Object fd_lock = new Object();     // for lock fd generation
    private final static Object cache_lock = new Object();  // for lock cache access
    private static final int EIO = -5;                      // errno
    private static final int EACCESS = -13;                 // errno
    private static final int MaxLen = 409600;               // Max length for chunking
    private static Integer fd = 7;                          // fd, inital fd with a rando number

    // map fd to path string
    private static Map<Integer, String> fd_path = new ConcurrentHashMap<>();
    
    // map fd to ras to read write flie in cache
    private static Map<Integer, RandomAccessFile> fd_map = new ConcurrentHashMap<>();
    
    public static String cache_dir;                          // the directory of cache  location
    public static int cache_size;                            // the size of the current cache
    public static Cache cache;                         // the cache object with LRU
    public static RemoteFile server;                        // the server RMI object

    private static class FileHandler implements FileHandling {

        /**
         * open: proxy open fuction
         * @ input param path:file path
         * @ input param OpenOption: open option
         * @ return fd or errno if error
         */
        public int open(String path, OpenOption o) {
            System.err.println("open path: "+ path + " start");
            System.err.println("OpenOption: "+ o.toString()+ " start");


            if (! (fd_map.size() < MAX_FILENUM) ) { 
                System.err.println("open has EMFILE error");
                return Errors.EMFILE; 
            }

            // check cache status then get current version
            path = mapPath(path);
            long crt_version = getVersion(path);
            boolean inCache;
            if (crt_version == -1){
                inCache = false;
            } else{
                inCache = true;
            }

            
            MyData new_file = get_file_data(path, crt_version, o);
            // if in cache, only get file's metadata, otherwise get data as well
            if (null == new_file) {
                return Errors.ENOENT;
            }

            // check the no such file and is_directory fault
            if (!new_file.exists() && (o == OpenOption.WRITE || o == OpenOption.READ)) {
                return Errors.ENOENT;
            } else if (new_file.isDir && o != OpenOption.CREATE_NEW && o != OpenOption.READ){
                return Errors.EISDIR;
            }

            // do the open operation
            int crt_fd;
            crt_fd = 0;
            path = cache_dir + path;
            switch (o) {
                case CREATE:
                    if (new_file.isError) return handle_error(new_file.ErrorMsg);
                    crt_fd = getFd();
                    return open_Create_file(crt_fd, path, new_file, crt_version);
                case CREATE_NEW:
                    // error handling
                    if (new_file.exists()) {
                     return Errors.EEXIST; 
                    }
                    if (new_file.isDirectory()) { 
                        return Errors.EISDIR;
                    }
                    if (new_file.isError) {
                        return handle_error(new_file.ErrorMsg);
                    }

                    crt_fd = getFd();
                    return open_CreateNew_file(crt_fd, path, new_file,crt_version);
                case READ:
                    if (new_file.isError) {
                        return handle_error(new_file.ErrorMsg);
                    }
                    // if path is a directory
                    if (new_file.isDirectory()) {
                        crt_fd = getFd();
                        fd_path.put(crt_fd, path);
                        return crt_fd;
                    }
                    // if path is a file
                    crt_fd = getFd();
                    return open_Read_file(crt_fd, path, new_file, crt_version);
               case WRITE:
                    if (new_file.isError) {
                        return handle_error(new_file.ErrorMsg);
                    }
                    crt_fd = getFd();
                    return open_Write_file(crt_fd, path, new_file, crt_version);
                default:
                    return Errors.EINVAL;
            }
        }


        /**
         * Write: write data to local cache file 
         * @input param fd
         * @input param buf: write buffer
         * @return number of bytes write or errno
         */
        public long write(int fd, byte[] buf) {
            // error handling
            if (!fd_path.containsKey(fd)) { 
                return Errors.EBADF;
            }

            File file;
            file = new File(fd_path.get(fd));

            if (!file.exists()) { 
                return Errors.ENOENT; 
            }
            if (file.isDirectory()) { 
                return Errors.EISDIR;
            }

            // write to cache file
            RandomAccessFile raf;
            raf = fd_map.get(fd);
            try {
                raf.write(buf);
                String name;
                name = fd_path.get(fd);
                long len;
                len = new File(name).length();

                // change length in cache
                synchronized (cache_lock) {
                    cache.set(name, (int) len);
                }
            } catch (IOException e) {
                e.printStackTrace();
                System.err.println(e.getMessage());
                if (e.getMessage().contains("Bad file descriptor")) 
                    return Errors.EBADF;
                else if (e.getMessage().contains("Permission")) 
                    return EACCESS;
                else if (e.getMessage().contains("directory")) 
                    return Errors.EISDIR;
                return EIO;
            }
            return buf.length;
        }


        /**
         * Perform read in proxy.
         * @input param fd
         * @input param buf
         * @return bytes read or errno
         */
        public long read(int fd, byte[] buf) {
            // error handling
            if (!fd_path.containsKey(fd)) { return Errors.EBADF; }
            File file = new File(fd_path.get(fd));
            if (!file.exists()) { return Errors.ENOENT;}
            if (file.isDirectory()) { return Errors.EISDIR;}

            RandomAccessFile raf = fd_map.get(fd);
            try {
                int read_num = raf.read(buf);
                if (read_num == -1) return 0;
                synchronized (cache_lock) {
                    cache.get(fd_path.get(fd));
                }
                return (long) read_num;
            } catch (IOException e) {
                e.printStackTrace();
                if (e.getMessage().contains("Bad file")) 
                    return Errors.EBADF;
                else if (e.getMessage().contains("Permission")) 
                    return EACCESS;
                else if (e.getMessage().contains("directory")) 
                    return Errors.EISDIR;

                return EIO;
            }
        }


        /**
         * Lseek operatino at cache file
         * @input param fd
         * @input param pos: offset
         * @input param o option
         * @return offset or errno
         */
        public long lseek(int fd, long pos, LseekOption o) {
            // error handling
            if (!fd_path.containsKey(fd)) 
                return (long)Errors.EBADF;

            String path;
            path = fd_path.get(fd);
            File file;
            file = new File(path);
            if (!file.exists())  
                return Errors.ENOENT;
            if (file.isDirectory()) 
                return Errors.EISDIR; 

            // get pos
            RandomAccessFile raf;
            raf = fd_map.get(fd);
            if (pos < 0) 
                return Errors.EINVAL;
            switch (o) {
                case FROM_CURRENT:
                    try {
                        pos = raf.getFilePointer() + pos;
                    } catch (IOException eio) {
                        eio.printStackTrace(); 
                        return EIO; 
                    }
                    break;
                case FROM_END:
                    try {
                        pos = raf.length() + pos;
                    } catch (IOException eio2) {
                        eio2.printStackTrace(); 
                        return EIO; 
                    }
                    break;
                case FROM_START:
                    break;
                default:
                    return Errors.EINVAL;
            }
            if (pos < 0) { 
                return Errors.EINVAL; 
            }

            // perform lseek
            try {
                raf.seek(pos);
                synchronized (cache_lock) { cache.get(fd_path.get(fd));}
                return pos;
            } catch (IOException e) {
                e.printStackTrace();
                return EIO;
            }
        }


        /**
         * To Unlink a "server" file
         * @input param path: file path
         * @return 0 fior success, errno for error
         */
        public int unlink(String path) {
            try {
                String state ;
                state = server.unlink(path);
                if (state == null) 
                    return 0;
                else if (state.equals("EACCESS")) 
                    return EACCESS;
                else if (state.equals("EIO")) 
                    return EIO;
                else if (state.equals("ENOENT")) 
                    return Errors.ENOENT;
                else if (state.equals("EPERM")) 
                    return Errors.EPERM;
                else if (state.equals("EBADF")) 
                    return Errors.EBADF;
                else return EIO;
            } catch (RemoteException e) {
                e.printStackTrace();
                return EIO;
            }
        }


        /**
         * Close a file in proxy.
         * If read only, decrease reference count in cache.
         * If write happens, write back data to server.
         * @input param fd
         * @return 0 for success, errno if error happens
         */
        public int close(int fd) {
            System.err.println("close "+ String.valueOf(fd)+" start");
            // Error handling
            if (!fd_path.containsKey(fd)) { 
                return Errors.EBADF; 
            }
            String path;
            path = fd_path.get(fd);
            File file;
            file = new File(path);
            if (!file.exists()) { 
                return Errors.ENOENT; 
            }

            // if path is a directory
            if (file.isDirectory()) {
                fd_path.remove(fd);
                return 0;
            }

            // if it is not read-only, then write back new version 
            if (!isReadOnly(path)) {
                path = path.substring(cache_dir.length());
                int index = path.lastIndexOf("_w", path.lastIndexOf("_w") - 1);
                if (index < 0) 
                    return EIO;

                try {
                    // if no chunking, then write back data using RPC
                    int len;
                    len = (int) file.length();
                    long version;
                    version = 0;
                    if (len <= MaxLen) {
                        path = path.substring(0, index);
                        byte[] data ;
                        data = new byte[len];

                        RandomAccessFile f = new RandomAccessFile(fd_path.get(fd), "r");
                        f.readFully(data, 0, len);
                        f.close();
                        MyData writeBack; 
                        writeBack = new MyData(len, data);
                        version = server.close(path, writeBack);
                        fd_map.get(fd).close();
                    }

                    // write back data using chunking
                    else {
                        int write;
                        write = 0;
                        long offset;
                        offset = 0;
                        RandomAccessFile f = new RandomAccessFile(fd_path.get(fd), "r");
                        byte[] buf ;
                        buf = new byte[MaxLen];
                        while (write < len) {
                            int read_num = f.read(buf);
                            offset = server.write(path, offset, buf, read_num);
                            if (offset == -1) 
                                return EIO;
                            write = write + read_num;
                        }

                        f.close();
                        version = server.close(path, path.substring(0, index));
                        path = path.substring(0, index);
                    }

                    // rename file to read version
                    file.renameTo(new File(cache_dir + path + "_r" + version));
                    synchronized (cache_lock) {
                        cache.setNewName(fd_path.get(fd), cache_dir + path + "_r" + version);
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                    return EIO;
                }
            } else {
                // if file is read-only data, decrease reference in cache
                try {
                    fd_map.get(fd).close();
                    synchronized (cache_lock) {
                        cache.decreaseReference(fd_path.get(fd), 1);
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                    return EIO;
                }
            }
            
            fd_path.remove(fd);
            fd_map.remove(fd);
            System.err.println(cache.toString());
            System.err.println("close "+ String.valueOf(fd)+" finish with no exception");
            return 0;
        }


        public void clientdone() {

        }

        /**
         * Open a file with WRITE flag.
         * @input param crt_fd: fd
         * @input param path: the file path
         * @input param new_file: the file's metadata
         * @input param crt_version: current version in cache
         * @return return fd or errno
         */
        private int open_Write_file(int crt_fd, String path, MyData new_file, long crt_version) {
            try {
                //if not in cache, make cache copy for this fd
                if (new_file.version != -1 || crt_version == -1 ) {
                    String orig_path;
                    orig_path = path;
                    path = path + "_w" + crt_fd + "_w" + new_file.version;
                    int state;
                    state = 0;
                    synchronized (cache_lock) {
                        state = cache.set(path, (int) new_file.len, 1);
                    }
                    if (state == -1) 
                        return Errors.EMFILE;
                    RandomAccessFile tmp = new RandomAccessFile(path, "rw");
                    state = readfile_from_server(tmp, new_file, orig_path);
                    if (state != 0) 
                        return state;
                } else { 
                // make a copy of cached file
                    String orig_path = path;
                    path = path + "_w" + crt_fd + "_w" + crt_version;
                    copyFileUsingFileStreams(orig_path + "_r" + crt_version, path);
                    synchronized (cache_lock) {
                        cache.set(path, (int) new File(path).length(), 1);
                    }
                }

                RandomAccessFile raf = new RandomAccessFile(path, "rw");
                fd_path.put(crt_fd, path);
                fd_map.put(crt_fd, raf);
                return crt_fd;
            } catch (FileNotFoundException e) {
                e.printStackTrace();
                if (e.getMessage().contains("Permission")) 
                    return Errors.EPERM;

                return Errors.ENOENT;
            } catch (SecurityException e) {
                return Errors.EPERM;
            } catch (IOException e) {
                return EIO;
            }
        }

        /**
         * Open a file with READ flag.
         * @input param crt_fd: fd
         * @input param path: the file path
         * @input param new_file: the file's metadata
         * @input param crt_version: current version in cache
         * @return the fd or errno
         */
        private int open_Read_file(int crt_fd, String path, MyData new_file, long crt_version) {
            try {
                // if not in cache, make cache copy 
                if (new_file.version != -1 || crt_version == -1 ) {
                    String orig_path;
                    orig_path = path;
                    path = path + "_r" + new_file.version;
                    RandomAccessFile tmp;
                    tmp = new RandomAccessFile(path, "rw");
                    int state;
                    state = readfile_from_server(tmp, new_file, orig_path);
                    if (state != 0) 
                        return state;
                    state = 0; // reset state for next use
                    synchronized (cache_lock) {
                        cache.deleteOldVersion(path);
                        state = cache.set(path, (int) new_file.len, 1);
                    }
                    if (state == -1) 
                        return Errors.EMFILE;
                }
                // get a cache file
                else {
                    path = path + "_r" + crt_version;
                    synchronized (cache_lock) {
                        cache.addReference(path, 1);
                    }
                }

                RandomAccessFile raf;
                raf = new RandomAccessFile(path, "r");
                fd_path.put(crt_fd, path);
                fd_map.put(crt_fd, raf);
                return crt_fd;
            } catch (FileNotFoundException e) {
                if (e.getMessage().contains("Permission")) 
                    return Errors.EPERM;

                return Errors.ENOENT;
            } catch (SecurityException e) {
                return Errors.EPERM;
            }
        }


        /**
         * Open a file with CREATE NEW flag.
         * @input param crt_fd: fd
         * @input param path: the file path
         * @input param new_file: the file's metadata
         * @input param crt_version: current version in cache
         * @return fd or errno
         */
        private int open_CreateNew_file(int crt_fd, String path, MyData new_file, long crt_version) {
            try {
                path = path + "_w" + crt_fd + "_w" + new_file.version;
                RandomAccessFile raf;
                raf = new RandomAccessFile(path, "rw");
                synchronized (cache_lock) {
                    cache.set(path, 0, 1);
                }
                fd_path.put(crt_fd, path);
                fd_map.put(crt_fd, raf);

                return crt_fd;
            } catch (FileNotFoundException e) {
                if (e.getMessage().contains("Permission")) 
                    return Errors.EPERM;
                return Errors.ENOENT;
            } catch (SecurityException e) {
                return Errors.EPERM;
            }
        }


        /**
         * Open a file with CREATE flag.
         * @input param crt_fd: fd
         * @input param path: the file path
         * @input param new_file: the file's metadata
         * @input param crt_version: the current version in cache
         * @return fd or errno
         */
        private int open_Create_file(int crt_fd, String path, MyData new_file, long crt_version) {
            try {
                // make cache copy for this fd if not in cache
                if (new_file.version != -1 || crt_version == -1 ) {
                    String orig_path;
                    orig_path = path;
                    path = path + "_w" + crt_fd + "_w" + new_file.version;
                    int state;
                    state = 0;
                    synchronized (cache_lock) {
                        state = cache.set(path, (int) new_file.len, 1);
                    }
                    if (state == -1) 
                        return Errors.EMFILE;
                    RandomAccessFile tmp ;
                    tmp = new RandomAccessFile(path, "rw");
                    state = readfile_from_server(tmp, new_file, orig_path);
                    if (state != 0) 
                        return state;
                } else {
                    // if in cache, make a new copy for write
                    String cache_path = path + "_r" + crt_version;
                    path = path + "_w" + crt_fd + "_w" + crt_version;
                    int state;
                    state = 0;
                    synchronized (cache_lock) {
                        state = cache.set(path, (int) new File(cache_path).length(), 1);
                    }
                    if (state == -1) 
                        return Errors.EMFILE;
                    copyFileUsingFileStreams(cache_path, path);
                }

                // put it in map
                RandomAccessFile raf;
                raf = new RandomAccessFile(path, "rw");
                fd_path.put(crt_fd, path);
                fd_map.put(crt_fd, raf);
                
                return crt_fd;
            } catch (FileNotFoundException e) {
                if (e.getMessage().contains("Permission")) 
                    return Errors.EPERM;
                return Errors.ENOENT;
            } catch (SecurityException e) {
                return Errors.EPERM;
            } catch (IOException e) {
                return EIO;
            }
        }

        /**
         * Read a file from server with chunking
         * @input param tmp: used for write to local copy
         * @input param new_file: file data from server
         * @input param orig_path: file's path
         * @return 0 on success, other for errors
         */
        private static int readfile_from_server(RandomAccessFile tmp, MyData new_file, String orig_path) {
            try {
                long total_len;
                long len ;

                total_len = new_file.len;
                len = new_file.data.length;

                tmp.write(new_file.data);
                new_file.flush();

                MyDataRead data;
                long offset ;
                offset = len;
                orig_path = orig_path.substring(cache_dir.length());
                while (len < total_len) {
                    data = server.read(orig_path, offset);
                    if (data == null) 
                        return EIO;
                    tmp.write(data.data);
                    len = len + data.size;
                    offset = data.offset;
                }
                tmp.close();
            } catch (IOException e) {
                return EIO;
            }
            new_file.flush();
            return 0;
        }


        /** from source, copy a file  to dest
         *  for when making a copy for write
         * @input param str1 the source file
         * @input param str2 the destination file
         * @throws IOException
         */
        private static void copyFileUsingFileStreams(String str1, String str2)
                throws IOException {
            File source ;
            File dest ;
            source= new File(str1);
            dest = new File(str2);
            if (!dest.exists()) 
                dest.createNewFile();
            InputStream input = null;
            OutputStream output = null;
            try {
                input = new FileInputStream(source);
                output = new FileOutputStream(dest);
                byte[] buf;
                buf = new byte[2046];
                int bytesRead;
                while ((bytesRead = input.read(buf)) > 0) {
                    output.write(buf, 0, bytesRead);
                }
            } finally {
                output.close();
                input.close();
                
            }
        }

        /**
         * Check if the path is read only
         * @input param path
         * @return
         */
        private static boolean isReadOnly(String path) {
            int index ;
            index = path.lastIndexOf("_r");
            if (index == -1) 
                return false;
            try {
                Long.parseLong(path.substring(index + 2));
            } catch (NumberFormatException e) {
                return false;
            }
            return true;
        }

        /**
         * this function is Used for handle error
         * @input param errorMsg
         * @return
         */
        private static int handle_error(String errorMsg) {
            if (errorMsg.contains("Permission")) 
                return Errors.EPERM;
            if (errorMsg.contains("Bad file descriptor")) 
                return Errors.EBADF;
            if (errorMsg.contains("No such")) 
                return Errors.ENOENT;
            return Errors.EBADF;
        }

        /**
         * Get a new file descriptor.
         * This method is synchronized by fd_lock object.
         * @return new fd.
         */
        private int getFd() {
            int crt_fd; 
            crt_fd = 0;
            synchronized (fd_lock) {
                fd = fd + 1;
                crt_fd = fd;
            }
            return crt_fd;
        }

        /**
         * from absolute path to client-side path
         * @input param orig_path
         * @return: the client side path
         */
        private static String mapPath(String orig_path) {
            return orig_path.replaceAll("/", "%`%");
        }

        /**
         * Get the current version of file in cache.
         * @input param path: the file path
         * @return -1 if not in cache, last-modified-timestamp if in cache
         */
        private long getVersion(String path) {
            long crt_version;
            crt_version = -1;
            synchronized (cache_lock) {
                crt_version = cache.checkVersion(cache_dir + path);
            }
            return crt_version;
        }

        /**
         * Get the  file metadata.
         * @input param path file path
         * @input param current_version cuurent version in cache
         * @input param Operation for open
         * @return MyData class contains file metadata
         */
        private MyData get_file_data(String path, long current_version, OpenOption o) {
            MyData new_file ;
            new_file = null;
            try {
                switch (o) {
                    case CREATE:
                        new_file = server.open(path, 1, current_version);
                        break;

                    case CREATE_NEW:
                        new_file = server.open(path, 2, current_version);
                        break;

                    case READ:
                        new_file = server.open(path, 3, current_version);
                        break;

                    case WRITE:
                        new_file = server.open(path, 4, current_version);
                        break;
                }

                return new_file;
                
            } catch (RemoteException e) {
                e.printStackTrace();
                return null;
            }
        }
    }

    private static class FileHandlingFactory implements FileHandlingMaking {
        public FileHandling newclient() {
            return new FileHandler();
        }
    }


    public static void main(String[] args) throws IOException {
        cache_size = Integer.parseInt(args[3]);   // the cache size
        cache_dir = args[2] + "/";                // the cache dir
        cache = new Cache(cache_size);       // set up new cache

        // bind a RMI service
        try {
            server = (RemoteFile) Naming.lookup("//" + args[0] +
                    ":" + args[1] + "/RemoteFile");         //objectname in registry 
            System.err.println("The Proxy is ready");
        } catch (Exception e) {
            System.err.println("The Client has exception: " + e.toString());
            e.printStackTrace();
        }

        System.err.println("The proxy start to run!");
        (new RPCreceiver(new FileHandlingFactory())).run();
    }
}

