import java.io.Serializable;

/**
 * This is a class for read the Serializable file data between proxy and server.
 */
public class MyDataRead implements Serializable {
	long offset = 0;     // current file pointer
	byte[] data = null;  // read file 
	int size = -1;       // file  size
	
	public MyDataRead(long offset, byte[] data, int size) {
		this.offset = offset;
		this.size = size;
		this.data = data;
	}
}
