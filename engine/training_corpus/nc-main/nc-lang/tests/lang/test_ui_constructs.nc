// Test: UI-related NC language constructs
// Verifies record creation, nested access, template patterns

service "test-ui-constructs"
version "1.0.0"

to test record creation:
    set card to {"title": "Hello", "body": "World", "color": "blue"}
    respond with card["title"]

to test nested record:
    set page to {"header": {"title": "NC", "subtitle": "Language"}}
    set hdr to page["header"]
    respond with hdr["title"]

to test record dot access:
    set config to {"theme": "dark", "font": "mono"}
    respond with config.theme

to test list of records:
    set items to [{"name": "A"}, {"name": "B"}, {"name": "C"}]
    respond with len(items)

to test record update:
    set style to {"color": "red", "size": 12}
    set style.color to "blue"
    respond with style.color

to test deep record update:
    set page to {"layout": {"columns": 2, "gap": 10}}
    set page.layout.columns to 3
    set lay to page["layout"]
    respond with lay["columns"]

to test bracket access:
    set data to {"font-size": "16px"}
    respond with data["font-size"]

to test record with list:
    set nav to {"items": ["Home", "About", "Contact"]}
    set menu to nav["items"]
    respond with menu[0]

to test template string:
    set name to "NC"
    set ver to "1.0"
    set result to name + " v" + ver
    respond with result

to test conditional style:
    set dark to true
    if dark is true:
        respond with "dark-theme"
    otherwise:
        respond with "light-theme"

to test list iteration:
    set total to 0
    set items to [10, 20, 30]
    repeat for each item in items:
        add item to total
    respond with total

to test record iteration:
    set count to 0
    set items to [{"v": 1}, {"v": 2}, {"v": 3}]
    repeat for each item in items:
        add 1 to count
    respond with count
