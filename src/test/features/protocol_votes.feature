Feature: The shareholders vote for protocol version updates
  Scenario: Nodes switch to v20
    Given a network with nodes "A" and "B" able to mint
    And a node "Old" running version "0.5.2" and able to mint
    And the nodes travel to the Nu protocol v05 switch time

    When node "A" finds 4 blocks received by all nodes
    And node "B" finds 5 blocks received by all nodes
    And node "Old" finds 1 blocks received by all nodes
    Then node "A" should use protocol 50000
    And node "B" should use protocol 50000

    When the nodes travel to the Nu protocol v20 switch time
    And node "B" finds 1 blocks received by all nodes
    And node "A" finds a block "v20"
    Then all nodes should be at block "v20"
    And node "A" should use protocol 2000000
    And node "B" should use protocol 2000000

    When node "Old" finds a block "V20_rejected"
    And node "A" finds a block "V20_accepted"
    And all nodes reach the same height
    Then nodes "A", "B" should be at block "V20_accepted"

  Scenario: Nodes switch to v20 after a majority
    Given a network with nodes "A" and "B" able to mint
    And a node "Old" running version "0.5.2" and able to mint
    And the nodes travel to the Nu protocol v05 switch time

    When node "A" finds 4 blocks received by all nodes
    And node "B" finds 4 blocks received by all nodes
    And node "Old" finds 2 blocks received by all nodes
    And the nodes travel to the Nu protocol v20 switch time
    And node "A" finds a block received by all other nodes
    Then node "A" should use protocol 50000
    And node "B" should use protocol 50000


    When node "A" finds 4 blocks received by all nodes
    And node "B" finds 5 blocks received by all nodes
    Then node "A" should use protocol 2000000
    And node "B" should use protocol 2000000
