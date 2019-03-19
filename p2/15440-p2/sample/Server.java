/**
 * This is a server class for concurrent remote file operation.
 * It is based on "Check-on-Use" cache protocal supporting open-close session semantics.
 * It extends RPC interface RemoteFile.
 *
 * Supports basic operaions such as open, close, write, read, unlink.
 * Every file has a ReentrantReadWriteLock to ensure atomic operation.
 *
 * When chunking data happens, it makes a shallow copy first and write back after all data received.
 *
 */

import java.io.*;
import java.util.*;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.rmi.registry.*;

import java.rmi.RemoteException;
import java.rmi.Naming;
import java.rmi.server.UnicastRemoteObject;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantReadWriteLock;


public class Server extends UnicastRemoteObject implements RemoteFile {
	public String rootdir = "";    // the root directory
	public static File root;       // the root directory file object

	// Map a file path to a ReentrantReadWriteLock 
	private Map<String, ReentrantReadWriteLock> locks = new ConcurrentHashMap<>();
	private static final Object map_lock = new Object();  // the lock obj for locks map
	private static final int MaxLen = 409600;             // set the maximum chunking size
	
	protected Server() throws RemoteException {
		super();
	}

	/**
	 * Close a file and write back the data updated. (with no chunking)
	 * @input param path
	 * @input param writeBack: write back data
	 * @return return the latest version number, -1 if error
	 * @throws RemoteException
     */
	public long close(String path, MyData writeBack) throws RemoteException {
		path = rootdir + get_orig_path(path);
		try {
			// get write lock
			if (locks.get(path) != null) {locks.get(path).writeLock().lock();
			} else {
				synchronized(map_lock) {

					if ( null == locks.get(path)) {locks.put(path, new ReentrantReadWriteLock());}

					locks.get(path).writeLock().lock();
				}
			}
			// write back
			FileOutputStream output ;
			output = new FileOutputStream(path, false);
			output.write(writeBack.data);
			output.close();
			return new File(path).lastModified();
		} catch (IOException e) {
			return -1;
		} finally {

			locks.get(path).writeLock().unlock();

		}
	}

	/**
	 * Close a file and write back the data updated. (with chunking)
	 * After all chunk received, copy from the shallow copy to the master copy .
	 * @input param tem_path: the shallow copy path
	 * @input param path: the master copy path
	 * @return the latest version number,return -1 when error
	 * @throws RemoteException
     */
	public long close(String tem_path, String path) throws RemoteException {
		path = rootdir + get_orig_path(path);
		tem_path = rootdir + get_orig_path(tem_path);
		
		try {
			// get the write lock
			if (locks.get(path) != null) {locks.get(path).writeLock().lock();
			} else {
				synchronized(map_lock) {
					if (locks.get(path) == null) {locks.put(path, new ReentrantReadWriteLock());}

					locks.get(path).writeLock().lock();
				}
			}
			// write back the shallow copy to master copy
			copyFileUsingFileStreams(tem_path, path);
			Path tmp;
			tmp = Paths.get(tem_path);
			Files.delete(tmp);
			return new File(path).lastModified();
		} catch (IOException e) {
			return -1;
		} finally {
			// release the lock
			locks.get(path).writeLock().unlock();
		}
	}


	/**
	 * To Unlink a file at server
	 * @input param path is the input path
	 * @return return Error message if error, null if success
	 * @throws RemoteException
     */
	public String unlink(String path) throws RemoteException {
		path = rootdir + path;
		File file;
		file = new File(path);

		// get write lock
		if (locks.get(path) != null) {
			locks.get(path).writeLock().lock();
		}

		// error handling
		if (!file.exists()) {

			if (locks.get(path) != null) {locks.get(path).writeLock().unlock();}
			return "ENOENT";
		}

		if (file.isDirectory()) {

			if (locks.get(path) != null) {locks.get(path).writeLock().unlock();} 

			return "EISDIR";
		}
		
		// delete files
		try {
			Path tmp ;
			tmp = Paths.get(path);
			Files.delete(tmp);
			return null;
		} catch (SecurityException e) {
			return "EPERM";
		} catch (NoSuchFileException e) {
			String errno = "ENOENT";
			if (e.getMessage().contains("Permission")) 
				errno = "EACCESS";
			return errno;
		} catch (IOException e) {
			if (e.getMessage().contains("Bad file descriptor")) 
				return "EBADF";
			else if (e.getMessage().contains("Permission")) 
				return "EACCESS";
			return "EIO";
		} finally {
			if (locks.get(path) != null) {
				locks.get(path).writeLock().unlock();
				synchronized(map_lock) {
					locks.remove(path);
				}
			} 
		}
	}

	/**
	 * Open a file at the path.
	 * Return the file's metadata. If new version is detected, return file data as well.
	 * @input param path: the file path
	 * @input param option: open flag(1-create, 2-createnew, 3-read, 4-write)
	 * @input param version: the cache latest version
	 * @return return MyData class contains file's metadata, null if not in rootdir
	 * @throws RemoteException
	 */
	public MyData open(String path, int option, long version) throws RemoteException {
		// if not in root directory
		path = rootdir + get_orig_path(path);
		File file ;
		file = new File(path);
		if (!isSubDirectory(file)) { 
			return null; 
		}
		MyData file_data ;
		file_data = new MyData(0, new byte[0]);

		// get read lock
		if (locks.get(path) != null) {
			locks.get(path).readLock().lock();
		} else {
			synchronized(map_lock) {
				if (locks.get(path) == null) {
					locks.put(path, new ReentrantReadWriteLock());
				}
				locks.get(path).readLock().lock();
			}
		}

		// if the file exist
		if (file.exists()) {
			file_data.isExist = true;

			// check for create_new return error
			if (option == 2) {
				file_data.isError = true;
				if (locks.get(path) != null) {
					locks.get(path).readLock().unlock();
				}
				return file_data;
			}

			// if the path is directory
			if (file.isDirectory()) {
				if (locks.get(path) != null) {
					locks.get(path).readLock().unlock();
				}
				file_data.isDir = true;
			} else {
				// if the path is not directory, try to read data 
				String mode = "rw";

				if (option == 3)  
					mode = "r";
				try {
					RandomAccessFile raf;
					raf = new RandomAccessFile(path, mode);
					long server_version;
					server_version = file.lastModified();
					// if newer version detected, return data as well
					if (server_version > version) {
						long size; 
						size = file.length();
						byte[] data;
					    data = new byte[MaxLen <= (int)size? MaxLen:(int)size];
						raf.read(data);
						raf.close();
						file_data.version = server_version;
						file_data.len = size;
						file_data.data = data;
					}
				} catch (FileNotFoundException e) {
					e.printStackTrace();
					file_data.isError = true;
					file_data.ErrorMsg = e.getMessage();
				} catch (SecurityException e) {
					e.printStackTrace();
					file_data.isError = true;
					file_data.ErrorMsg = e.getMessage();
				} catch (IOException e) {
					e.printStackTrace();
					file_data.isError = true;
					file_data.ErrorMsg = e.getMessage();
				} finally {
					if(locks.get(path) != null) 
						{locks.get(path).readLock().unlock();}
				}
			}
		}

		// if the file not exist
		else {
			file_data.isExist = false;

			// if the file is not exist, return error for read and write
			if (option >= 3) {
				if (locks.get(path) != null) {
					locks.get(path).readLock().unlock();
				}
				return file_data;
			}

			// if is directory
			if (file.isDirectory()) {
				if (locks.get(path) != null) {
					locks.get(path).readLock().unlock();
				}
				file_data.isDir = true;
			} else {
				// create new file if it is not directory
				if (option <= 2) {
					try { file.createNewFile();
					} catch (IOException e) {
						file_data.isError = true;
						file_data.ErrorMsg = e.getMessage();
					} finally {
						if(locks.get(path) != null) {
							locks.get(path).readLock().unlock();
						}
					}
					file_data.len = file.length();
					file_data.version = file.lastModified();
				}
			}
		}

		return file_data;
	}

	/**
	 * Read the data from a file at offset, maximum reading size MaxLen
	 * Used for read chunking .
	 * @input param path: the file path
	 * @input param offset: the file pointer
	 * @return return the MyDataRead class contains data read
	 * @throws RemoteException
     */
	@Override
	public MyDataRead read(String path, long offset) throws RemoteException {
		path = rootdir + get_orig_path(path);

		try {

			//get the read lock
			synchronized(map_lock) {
				if (locks.get(path) == null) {locks.put(path, new ReentrantReadWriteLock());}
				locks.get(path).readLock().lock();

			}

			// read data from path
			RandomAccessFile raf;
			raf = new RandomAccessFile(path, "rw");
			raf.seek(offset);
			byte[] buf;
			buf = new byte[MaxLen];
			int size ;
			size = raf.read(buf, 0, MaxLen);
			offset = raf.getFilePointer();
			raf.close();
			if (size < MaxLen) 
				return new MyDataRead(offset, Arrays.copyOf(buf, size), size);

			return new MyDataRead(offset, buf, size);
		} catch (IOException e) {
			e.printStackTrace();
		} finally {
			if (locks.get(path) != null) {locks.get(path).readLock().unlock();}
		}

		return null;
	}


	/**
	 * Write back the data to a file at offset from buf of size bytes
	 * Used for chunking write back to the shallow copy.
	 * @input param path
	 * @input param offset: the file pointer
	 * @input param buf the data buffer
	 * @input param size: the buffer size
	 * @return next file pointer after write
	 * @throws RemoteException
     */
	@Override
	public long write(String path, long offset, byte[] buf, int size) throws RemoteException {
		path = rootdir + get_orig_path(path);
		try {
			RandomAccessFile raf ;
			raf = new RandomAccessFile(path, "rw");
			raf.seek(offset);
			raf.write(buf, 0, size);;
			offset = raf.getFilePointer();
			raf.close();
			return offset;
		} catch (IOException e) {
			e.printStackTrace();
			return -1;
		} 
	}


	/**
	 * Copy the file from path string1 to path string2
	 * @input param str1: the first file path
	 * @input param str2: the second file path
	 * @throws IOException
     */
	private static void copyFileUsingFileStreams(String str1, String str2)
			throws IOException {
		File source;
		source =new File(str1);
		File dest;
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
	 * =check if a file belongs to root directory
	 * @input param the file object
	 * @return return true if in the directory, false otherwise
     */
	private boolean isSubDirectory(File file) {
		File tmp;
		try {
			tmp = file.getCanonicalFile();
		} catch (IOException e) {
			return false;
		}

		while (null != tmp) {
			if (root.equals(tmp)) { 
				return true; 
			}
			tmp = tmp.getParentFile();
		}
		return false;
	}


	/**
	 * Map the client side path with canonical path
	 * @input param new_path: client side path
	 * @return: the server side path
     */
	private static String get_orig_path(String new_path) {
        String result = new_path.replaceAll("%`%", "/"); 
		return result;

	}
	
	
    public static void main(String args[]) {
        try {
        	 // Bind the remote object's stub in the registry
        	Server server ;
        	server = new Server();
        	LocateRegistry.createRegistry(Integer.parseInt(args[0]));
            Registry registry;
            registry = LocateRegistry.getRegistry(Integer.parseInt(args[0]));
            registry.bind("RemoteFile", server);

			// root directory setup
            server.rootdir = args[1] + "/";
            server.root = new File(server.rootdir).getCanonicalFile();
            System.err.println("The server ready, rootdir:" + args[1]);
        } catch (Exception e) {
            System.err.println("The server exception: " + e.toString());
            e.printStackTrace();
        }
    }
}
