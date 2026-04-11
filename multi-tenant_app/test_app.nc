to test_name:, set result to func(), assert condition message, respond with ok.

to test_health_check:
    set result to health_check()
    assert result.status is equal "healthy", "Should be healthy"
    respond with "ok"

