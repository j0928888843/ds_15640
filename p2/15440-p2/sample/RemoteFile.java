/**
 * This is a interface which extend Remote, and it is extent for file operation of both server and client.
 *
 */
import java.util.*;
import java.io.RandomAccessFile;
import java.rmi.Remote;
import java.rmi.RemoteException;


public interface RemoteFile extends Remote {
	 MyData open(String path, int option, long version) throws RemoteException;
	 long close(String tem_path, String path) throws RemoteException;
	 long close(String path, MyData writeBack) throws RemoteException;
	 String unlink(String path) throws RemoteException;
	 long write(String path, long offset, byte[] buf, int size) throws RemoteException;
	 MyDataRead read(String path, long offset) throws RemoteException;
}
