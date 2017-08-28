# JRediSearch

A Java Client Library for [ReBloom](https://rebloom.io)

## Overview 

This project contains a Java library abstracting the API of the ReBloom Redis module, that implements a high
perfomance bloom filter with an easy-to-use API
 
See [http://rebloom.io](http://rebloom.io) for installation instructions of the module.

## Usage example

Initializing the client:

```java
import io.rebloom.client.Client

Client client = new Client("localhost", 6378);
```

Adding items to a bloom filter (created using default settings):

```java
client.add("simpleBloom", "Mark");
// Does "Mark" now exist?
client.exists("simpleBloom", "Mark"); // true
client.exists("simpleBloom", "Farnsworth"); // False
```


Use multi-methods to add/check multiple items at once:

```java
client.addMulti("simpleBloom", "foo", "bar", "baz", "bat", "bag");

// Check if they exist:
boolean[] rv = client.existsMulti("simpleBloom", "foo", "bar", "baz", "bat", "mark", "nonexist");
```

Reserve a customized bloom filter:

```java
client.createFilter("specialBloom", 10000, 0.0001);
client.add("specialBloom", "foo");

```