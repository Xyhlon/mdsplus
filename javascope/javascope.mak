..\java\classes\MDSplus.jar : jScope.class
	%JDK_DIR%\bin\jar.exe -cvf ..\java\classes\MDSplus.jar *.class

jScope.class : *.java
	%JDK_DIR%\bin\javac.exe -classpath MRJclasses.zip *.java

