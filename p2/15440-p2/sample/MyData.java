import java.io.Serializable;


/**
 * This is a class for the serializable file data for transmit between proxy and server.
 */
public class MyData implements Serializable {
	public long len;                  // data size
	public String ErrorMsg;           // file error message
	public byte[] data;               // file data
	public long version = -1;         // file version

	public boolean isExist = false;   // file exist
	public boolean isDir = false;     // file is directory?
	public boolean isError = false;   // file error occurs?

	
	/**
	 * drop the data payload.
	 */
	public void flush() {
		data = null;
	}

	public MyData(long len, byte[] data) {
		this.len = len;
		this.data = data;
	}

	/**
	 * Set an error flag and error message
	 * @param error
     */
	public void setError(String error) {
		this.isError = true;
		this.ErrorMsg = error;
	}


	/**
	 * To check if the file path is a directory
	 * @return true or false
     */
	public boolean isDirectory() { 
		return this.isDir; 
	}

	/**
	 * To check if the file path exists
	 * @return
     */
	public boolean exists() { 
		return this.isExist; 
	}



}

