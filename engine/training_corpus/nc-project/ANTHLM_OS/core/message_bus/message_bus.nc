// ══════════════════════════════════════════════════════════════════
//  HiveANT — Distributed Message Bus
//
//  Enables inter-agent communication. Agents publish/subscribe
//  to topics. Supports broadcast, direct, and cluster messaging.
// ══════════════════════════════════════════════════════════════════

to bus_publish with topic, message, sender_id:
    purpose: "Publish a message to a topic"
    shell("mkdir -p agents_state/bus")
    set msg to {"topic": topic, "message": message, "sender": sender_id, "timestamp": time_now(), "id": "MSG-" + str(floor(random() * 90000 + 10000))}
    set topic_file to "agents_state/bus/topic-" + topic + ".json"

    if file_exists(topic_file):
        set existing to read_file(topic_file)
        set msgs to json_decode(existing)
    otherwise:
        set msgs to {"messages": [], "subscriber_count": 0}

    set msgs.messages to msgs.messages + [msg]
    write_file(topic_file, json_encode(msgs))
    log "BUS: Published to " + topic + " by " + str(sender_id)
    respond with {"published": true, "topic": topic, "msg_id": msg.id}

to bus_subscribe with topic, agent_id:
    purpose: "Subscribe an agent to a topic"
    shell("mkdir -p agents_state/bus")
    set sub_file to "agents_state/bus/subs-" + topic + ".json"
    if file_exists(sub_file):
        set subs to json_decode(read_file(sub_file))
    otherwise:
        set subs to {"topic": topic, "subscribers": []}

    set subs.subscribers to subs.subscribers + [agent_id]
    write_file(sub_file, json_encode(subs))
    respond with {"subscribed": true, "topic": topic, "agent_id": agent_id}

to bus_read with topic, since:
    purpose: "Read messages from a topic"
    set topic_file to "agents_state/bus/topic-" + topic + ".json"
    if file_exists(topic_file):
        set data to json_decode(read_file(topic_file))
        respond with data
    otherwise:
        respond with {"messages": [], "topic": topic}

to bus_direct with target_agent, message, sender_id:
    purpose: "Send a direct message to a specific agent"
    shell("mkdir -p agents_state/bus/direct")
    set msg to {"message": message, "sender": sender_id, "timestamp": time_now()}
    set inbox to "agents_state/bus/direct/" + target_agent + ".json"
    if file_exists(inbox):
        set existing to json_decode(read_file(inbox))
    otherwise:
        set existing to {"inbox": []}
    set existing.inbox to existing.inbox + [msg]
    write_file(inbox, json_encode(existing))
    respond with {"sent": true, "to": target_agent}

to bus_topics:
    purpose: "List all active topics"
    set result to shell("ls agents_state/bus/topic-*.json 2>/dev/null | while read f; do basename \"$f\" .json | sed 's/topic-//'; done || echo NONE")
    if result is equal "NONE":
        respond with {"topics": [], "count": 0}
    set topics to split(trim(result), "\n")
    respond with {"topics": topics, "count": len(topics)}
