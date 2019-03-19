/**
 * The cache class for LRU cache.
 *
 * Use the LinkedHashMap to maintain LRU order.
 * Supports LRU operations including set, get, checkVersionNumer.
 *
 */

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

import java.util.*;

@SuppressWarnings("unchecked")


public class Cache {
	private LinkedHashMap<String, Entry> map;  // LRU cache
	private final int capacity;                //capacity in byte
	private int total;                         //total used bytes
	
	public Cache(int capacity) {

		this.capacity = capacity;
		//creat a LinkedHashMap to map path to Entry
		map = new LinkedHashMap<String, Entry>(16, 0.75f, true);
		
	}


	/**
	 * check the current cached version number in cache
	 * @input_param path
	 * @return timestamp of current version, -1 if not exists
	 */
	public long checkVersion(String path) {
		path = path + "_r";
		long result;
		result = -1;  // current version

		Iterator i ;
		i = map.entrySet().iterator();
		ArrayList<Long> old_version = new ArrayList<>();

		// iterator cache list find current version
		while (i.hasNext()) {
			Map.Entry entry;
			entry = (Map.Entry) i.next();
			String name;
			name = (String) entry.getKey();
			if (name.startsWith(path)) {
				long tmp;
				tmp = Long.parseLong(name.substring(name.lastIndexOf("_r") + 2));
				if (tmp > result) {
					if (result != -1) {
						if (map.get(path + result).reference == 0)
							old_version.add(result);
					}
					result = tmp;
				}
			}
		}

		// delete any older version with no reference
		for (long version : old_version) {
			Path tmp = Paths.get(path + version);
			try {
				Files.delete(tmp);
			} catch (IOException e) {
				e.printStackTrace();
			}
			int value;
			value = map.get(path + version).len;
			map.remove(path + version);
			total -= value;
		}
		
		return result;
	}


	/**
	 * Set a cache and move it to head by get(), and then  update cache length
	 * Update file if exist,if not exist, insert a new one 
	 * @input_param key:file path
	 * @input_param value: length
	 * @input_param reference: reference count, -1 when error
	 */
	public int set(String key, int value, int reference) {
		if (map.containsKey(key)) {
			Entry entry;
			entry = (Entry) map.get(key);
			total = total - entry.len;
			if (canPut(value)) {
				entry.len = value;
				entry.reference = reference;
				map.put(key, entry);
				total = total + value;
				return 0;
			} else {
				total = total + entry.len;
				return -1;
			}
		} else {
			if (canPut(value)) {
				Entry entry;
				entry = new Entry(value, reference, key);
				map.put(key, entry);
				total = total + value;
				return 0;
			} else {
				return -1;
			}
		}
	}





	/**
	 * add the reference number to an existing file when open
	 * @input_param key: cached file path
	 * @input_param reference: reference number change
	 */
	public void addReference(String key, int reference) {
		if (map.get(key) != null) {

			((Entry) map.get(key)).reference += reference;

		}
	}


	/**
	 * Set a cache to a new length value after write
	 * @input_param key: file path
	 * @input_param value: 0 if success, -1 if failure
	 */
	public int set(String key, int value) {
		if (map.containsKey(key)) {
			Entry entry = (Entry) map.get(key);
			total = total - entry.len;
			if (canPut(value)) {
				entry.len = value;
				map.put(key, entry);
				total = total + value;
				return 0;
			} else {
				total = total + entry.len;
				return -1;
			}
		} else {
			if (canPut(value)) {
				Entry entry;
				entry = new Entry(value, 1, key);
				total = total + value;
				map.put(key, entry);
				return 0;
			} else {
				return -1;
			}
		}
	}


	/**
	 * decrease reference number when close a file,
	 * delete itself if new version exists
	 * @input_param key: file path
	 * @input_param reference: reference number change
     */
	public void decreaseReference(String key, int reference) {
		if (map.get(key) != null) {
			((Entry) map.get(key)).reference -= reference;
		}
		if (map.get(key).reference != 0) return;
		
		// delete old version if new version exist
		String prefix = key.substring(0, key.lastIndexOf("_r") + 2);
		Iterator i = map.entrySet().iterator();
		while (i.hasNext()) {
			Map.Entry map_entry;
			map_entry = (Map.Entry) i.next();
			Entry entry;
			entry = (Entry) map_entry.getValue();
			if ( entry.key.compareTo(key) > 0 && entry.key.startsWith(prefix) ) {
				Path tmp;
				tmp = Paths.get(key);
				try {
					Files.delete(tmp);
					int value = map.get(key).len;
					map.remove(key);
					total = total - value;
					break;
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}
	}


	/**
	 * Use to delete the old version of a file
	 * @input_param newName: current file with timestamp
     */
	public void deleteOldVersion (String newName) {
		// delete old version
		String prefix;
		prefix = newName.substring(0, newName.lastIndexOf("_r") + 2);
		Iterator i;
		i = map.entrySet().iterator();
		while (i.hasNext()) {
			Map.Entry map_entry;
			map_entry = (Map.Entry) i.next();
			Entry entry;
			entry = (Entry) map_entry.getValue();

			// delete file of old version when no reference to this file
			if (entry.reference == 0 && entry.key.startsWith(prefix) && entry.key.compareTo(newName) < 0) {
				Path tmp;
				tmp = Paths.get(entry.key);
				try {
					Files.delete(tmp);
					int value = entry.len;
					i.remove();
					total -= value;
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}
	}

	
	/**
	 * Set  new name to cached file after write back
	 * @ input_param key: old cached file path
	 *            
	 * @input_param newName: new file path
	 *            
	 */
	public void setNewName(String key, String newName) {
		if (map.get(key) != null) {
			Entry entry;
			entry = (Entry) map.get(key);
			entry.key = newName;
			entry.reference = 0;
			map.remove(key);
			map.put(newName, entry);
		}

		// delete the old version
		deleteOldVersion(newName);
	}


	/**
	 * Return if len byte can be inserted so that won't exceed capacity
	 * @input_param len: file length
	 * @return true if can be inserted, false otherwise
	 */
	public boolean canPut(int len) {
		// if cache not full
		if (total + len <= capacity)
			return true;

		// check how many can be delete
		Iterator i;
		i = map.entrySet().iterator();
		ArrayList<String> list = new ArrayList<>();
		int deleted = 0;
		while (i.hasNext()) {
			Map.Entry map_entry;
			map_entry = (Map.Entry) i.next();
			Entry entry;
			entry = (Entry) map_entry.getValue();
			if (entry.reference == 0) {
				list.add((String) map_entry.getKey());
				deleted += entry.len;
				if ((total - deleted + len) <= capacity)
					break;
			}
		}

		// if can delete enough data, delete here, return true
		if ((total - deleted + len) <= capacity) {
			for (String key : list) {
				map.remove(key);
				Path tmp = Paths.get(key);
				try {
					Files.delete(tmp);
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
			total = total - deleted;
			return true;
		}

		// cannot insert such file now
		return false;
	}



	/**
	 * Readable representation of LRU cache
	 * @return String representation of LRU cache
     */
	public String toString() {
		StringBuilder sb;
		sb = new StringBuilder();
		sb.append("-- The Cache capacity:" + capacity + "  --The Cache length:" + total + "\n");

		// display cache from LRU to MRU
		Iterator i;
		i = map.entrySet().iterator();
		ArrayList<String> list = new ArrayList<String>();
		int deleted = 0;
		while (i.hasNext()) {
			Map.Entry map_entry = (Map.Entry) i.next();
			Entry entry = (Entry) map_entry.getValue();
			sb.append("[ " + map_entry.getKey() + " : LENGTH: " + entry.len + " REFERENCE: " + entry.reference + "] \n");
		}

		return sb.toString();
	}
	
	/**
	 * Used to get the cache entry and insert it to head.
	 * @input_param path: file path
	 */
	public void get(String path) {
		map.get(path);
	}



	/*
	 * Cache Entry class, used to record cache metadata
	 */
	class Entry {
		public int len;        // cache length
		public int reference;  // cache reference count
		public String key;     // cache file path

		public Entry(int len, int reference, String key) {
			this.key = key;
			this.len = len;
			this.reference = reference;
		}
	}
}