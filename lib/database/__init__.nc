// NC Standard Library — database
// Connect to any database

service "nc.database"
version "1.0.0"
description "Database operations for any backend"

to query with sql:
    purpose: "Run a SQL query"
    gather result from database:
        query: sql
    respond with result

to find with collection and filter:
    purpose: "Find documents matching a filter"
    gather result from mongodb:
        collection: collection
        query: filter
    respond with result

to search with index and text:
    purpose: "Search for text in an index"
    gather result from elasticsearch:
        index: index
        query: text
    respond with result

to cache_get with key:
    purpose: "Get a value from cache"
    gather result from redis:
        query: key
    respond with result

to cache_set with key and value:
    purpose: "Store a value in cache"
    store value into "redis:" + key
    respond with true

to insert with table and data:
    purpose: "Insert a row into a table"
    store data into table
    respond with true

to health:
    purpose: "Check database connectivity"
    respond with "connected"
