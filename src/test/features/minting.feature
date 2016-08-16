Feature: Test nodes can mint
  Scenario:
    Given a network with node "Alice" able to mint
    And a node "Bob"
    When node "Alice" finds a block "X"
    Then node "Alice" should be at block "X"
    And node "Bob" should be at block "X"
