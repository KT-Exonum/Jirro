#!/bin/sh

# Gradle wrapper script

APP_NAME="Gradle"
GRADLE_VERSION="8.6"

# Determine the script's directory
PRG="$0"
while [ -h "$PRG" ] ; do
    ls=`ls -ld "$PRG"`
    link=`expr "$ls" : '.*-> \(.*\)$'`
    if expr "$link" : '/.*' > /dev/null; then
        PRG="$link"
    else
        PRG=`dirname "$PRG"`"/$link"
    fi
done
SAVEDIR=`pwd`
cd `dirname "$PRG"`/.. > /dev/null
APP_HOME=`pwd`
cd "$SAVEDIR" > /dev/null

CLASSPATH=$APP_HOME/gradle/wrapper/gradle-wrapper.jar

# Find Java
if [ -n "$JAVA_HOME" ] ; then
    JAVA_EXE="$JAVA_HOME/bin/java"
else
    JAVA_EXE="java"
fi

if [ ! -x "$JAVA_EXE" ] ; then
    echo "ERROR: JAVA_HOME is not set and no 'java' command could be found in your PATH." >&2
    exit 1
fi

exec "$JAVA_EXE" \
    -classpath "$CLASSPATH" \
    -Dapp.name="$APP_NAME" \
    -Dapp.version="$GRADLE_VERSION" \
    org.gradle.wrapper.GradleWrapperMain \
    "$@"
