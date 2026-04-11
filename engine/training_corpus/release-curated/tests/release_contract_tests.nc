to sample_account:
    purpose: "Return a representative account fixture"
    respond with {
        "account_id": "acc_1001",
        "name": "Acme Labs",
        "plan": "growth",
        "status": "active",
        "owner_email": "owner@acme.com"
    }

to create_account with name, owner_email, plan:
    purpose: "Return a fixture that matches the enterprise account contract"
    if plan is equal "":
        set selected_plan to "growth"
    otherwise:
        set selected_plan to plan
    respond with {
        "account": {
            "account_id": "acc_1001",
            "name": name,
            "owner_email": owner_email,
            "plan": selected_plan,
            "status": "active"
        }
    }

to list_accounts with status, owner_email:
    purpose: "Return a plural collection shape for release tests"
    set account to sample_account()
    respond with {
        "accounts": [account],
        "count": 1,
        "filters": {"status": status, "owner_email": owner_email}
    }

to summarize_account with account_id:
    purpose: "Return a summary fixture for account review flows"
    respond with {
        "account_id": account_id,
        "headline": "Healthy renewal candidate",
        "health": "green"
    }

to classify_support_ticket with ticket_text, customer_tier:
    purpose: "Return a support triage fixture for release routing"
    respond with {
        "label": "release-blocker",
        "priority": "high",
        "summary": "Migration issue needs SDK review",
        "next_team": "sdk",
        "customer_tier": customer_tier
    }

describe "Enterprise Accounts Contracts":
    it "should create an account with the default plan":
        set result to create_account("Acme Labs", "owner@acme.com", "")
        assert result.account.name is equal "Acme Labs"
        assert result.account.plan is equal "growth"

    it "should list accounts with a plural response shape":
        set result to list_accounts("active", "")
        assert result.count is above 0
        assert result.accounts is not equal ""

    it "should return a summary for one account":
        set result to summarize_account("acc_1001")
        assert result.account_id is equal "acc_1001"
        assert result.health is equal "green"

describe "Release Assistant Contracts":
    it "should classify a release blocker ticket":
        set result to classify_support_ticket("Our SDK integration fails after the latest upgrade.", "enterprise")
        assert result.label is equal "release-blocker"
        assert result.priority is equal "high"
        assert result.next_team is equal "sdk"