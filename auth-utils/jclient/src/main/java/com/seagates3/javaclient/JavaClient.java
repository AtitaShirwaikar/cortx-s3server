/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Arjun Hariharan <arjun.hariharan@seagate.com>
 * Original creation date: 11-Feb-2016
 */
package com.seagates3.javaclient;

import com.google.gson.Gson;
import com.google.gson.internal.LinkedTreeMap;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import org.apache.commons.cli.BasicParser;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.log4j.Level;
import org.apache.log4j.LogManager;

public class JavaClient {

    private static final String CLASS_ACTION_FILE_NAME = "/class_action.json";
    private static final String PACKAGE_NAME = "com.seagates3.javaclient";
    private static Options s3Options;
    private static CommandLine cmd;

    public static void main(String[] args) throws Exception {
        JavaClient.run(args);
    }

    public static void run(String[] args) throws Exception {
        init(args);
        if (cmd.hasOption("h")) {
            showUsage();
        }

        if (cmd.hasOption("L")) {
            setLogLevel();
        }

        String[] commandArguments = cmd.getArgs();
        if (commandArguments.length == 0) {
            System.err.println("Incorrect command. Check usage");
            System.exit(1);
        }

        InputStream in = JavaClient.class.
                getResourceAsStream(CLASS_ACTION_FILE_NAME);
        InputStreamReader reader = new InputStreamReader(in, "UTF-8");

        Gson gson = new Gson();
        HashMap<String, Object> gsonObj = gson.fromJson(reader, HashMap.class);

        LinkedTreeMap<String, String> classActionMapping
                = (LinkedTreeMap<String, String>) gsonObj.get(commandArguments[0]);

        if (classActionMapping == null) {
            System.err.println("Incorrect command");
            showUsage();
        }

        String classPath = getClassPath(classActionMapping.get("Class"));
        Class<?> clazz;
        Constructor<?> classConstructor;
        Method method;
        Object obj;

        try {

            clazz = Class.forName(classPath);
            classConstructor = clazz.getConstructor(CommandLine.class);
            obj = classConstructor.newInstance(cmd);

            method = clazz.getMethod(classActionMapping.get("Action"));
            method.invoke(obj);
        } catch (ClassNotFoundException | NoSuchMethodException | SecurityException ex) {
            System.out.println(ex);
        } catch (IllegalAccessException | IllegalArgumentException |
                InvocationTargetException | InstantiationException ex) {
            System.out.println(ex);
        }

    }

    private static void init(String[] args) {
        s3Options = constructOptions();
        try {
            cmd = new BasicParser().parse(s3Options, args);
        } catch (ParseException ex) {
            System.err.println("Incorrect command.\n");
            showUsage();
        }
    }

    private static void setLogLevel() throws Exception {
        String logLevel = cmd.getOptionValue("L");

        Level level = Level.toLevel(logLevel);
        if (level == null) {
            throw new Exception("Incorrect logging level");
        }

        LogManager.getRootLogger().setLevel(level);
    }

    /**
     * Construct the options required for s3 java cli.
     *
     * @return Options
     */
    private static Options constructOptions() {
        Options options = new Options();
        options.addOption("x", "access_key", true, "Access key id")
                .addOption("y", "secret_key", true, "Secret key")
                .addOption("t", "session_token", true, "Session token")
                .addOption("p", "path_style", false, "Use Path style APIs")
                .addOption("l", "location", true, "Bucket location")
                .addOption("m", "multi_part_chunk_size", true,
                        "Size of chunk in MB")
                .addOption("a", "aws", false, "Run operation on AWS S3 (only for debugging)")
                .addOption("L", "log_level", true, "Log level (INFO, DEFAULT, "
                        + "ALL, WARN, ERROR, TRACE). Default ERROR.")
                .addOption("C", "chunk", false, "Enable chunked upload")
                .addOption("h", "help", false, "Show usage");

        return options;
    }

    private static void showUsage() {
        String usage = "Commands:\n"
                + "  Make bucket\n"
                + "      java -jar jclient.jar mb s3://BUCKET\n"
                + "  Remove bucket\n"
                + "      java -jar jclient.jar rb s3://BUCKET\n"
                + "  List objects or buckets\n"
                + "      java -jar jclient.jar ls [s3://BUCKET[/PREFIX]]\n"
                + "  Put objects into bucket\n"
                + "      java -jar jclient.jar put FILE s3://BUCKET[/PREFIX]\n"
                + "  Get objects from bucket\n"
                + "      java -jar jclient.jar get s3://BUCKET/OBJECT LOCAL_FILE\n"
                + "  Delete objects from bucket\n"
                + "      java -jar jclient.jar del s3://BUCKET/OBJECT\n"
                + "  Delete multiple objects from bucket\n"
                + "      java -jar jclient.jar multidel s3://BUCKET OBJECT1 Object2... ObjectN\n"
                + "  Head Object\n"
                + "      java -jar jclient.jar head s3://BUCKET/PREFIX/OBJECT\n"
                + "  Bucket or Object exists\n"
                + "      java -jar jclient.jar exists s3://BUCKET[/PREFIX/OBJECT]\n"
                + "  Show multipart uploads\n"
                + "      java -jar jclient.jar multipart s3://BUCKET\n"
                + "  Abort a multipart upload\n"
                + "      java -jar jclient.jar abortmp s3://BUCKET/OBJECT Id\n"
                + "  List parts of a multipart upload\n"
                + "      java -jar jclient.jar listmp s3://BUCKET/OBJECT Id"
                + "  Multipart upload\n"
                + "      java -jar jclient.jar put FILE s3://BUCKET[/PREFIX] "
                + "-m [size of each chunk in MB]\n"
                + "  Partial multipart upload (for System Tests only)\n"
                + "      java -jar jclient.jar partialput FILE s3://BUCKET NO_OF_PARTS_TO_UPLOAD[/PREFIX] "
                + "-m [size of each chunk in MB]\n";

        HelpFormatter s3HelpFormatter = new HelpFormatter();

        s3HelpFormatter.printHelp(usage, s3Options);
        System.exit(0);
    }

    private static String getClassPath(String className) {
        return PACKAGE_NAME + "." + className;
    }
}
