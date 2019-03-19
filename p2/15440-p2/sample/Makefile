all: MyData.class RemoteFile.class Server.class Cache.class Proxy.class

%.class: %.java
	javac $<

clean:
	rm -f *.class
handin:
	tar czvf ../mysolution.tgz Makefile *.java *.pdf
