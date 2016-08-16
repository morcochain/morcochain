Feature:
  The network should handle a fork that happens before a custodian grant and is resolved after it.
  Keeping track of the already elected custodians may make this fail.

  Scenario: The network forks around a custodian grant
    Given a network with nodes "Alice" and "Bob" able to mint
    And a node "Custodian"
    When node "Custodian" generates a NuBit address "cust"
    And node "Bob" ignores all new blocks
    And node "Alice" finds 3 blocks
    And node "Alice" votes an amount of "1,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And node "Bob" stops ignoring new blocks
    And node "Bob" votes an amount of "1,000" for custodian "cust"
    And node "Bob" finds blocks until custodian "cust" is elected
    And node "Bob" finds 5 blocks
    And node "Bob" finds a block "X"
    Then node "Alice" should reach block "X"

  Scenario: A custodian is elected but a reorganization removes the grant
    Given a network with nodes "Alice" and "Bob" able to mint
    And a node "Custodian"
    When node "Custodian" generates a NuBit address "cust"
    And node "Bob" ignores all new blocks
    And node "Alice" finds 3 blocks
    And node "Alice" votes an amount of "1,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And node "Custodian" reaches the same height as node "Alice"
    And node "Custodian" reaches a balance of "1000" NBT
    And node "Custodian" sends a liquidity of "1000" buy and "2000" sell on unit "B" from address "cust" with identifier "1:"
    Then node "Alice" should reach a total liquidity info of "1000" buy and "2000" sell on unit "B"
    And node "Bob" should reach a total liquidity info of "0" buy and "0" sell on unit "B"

    When node "Bob" stops ignoring new blocks
    And node "Bob" finds blocks until it reaches a higher trust than node "Alice"
    And node "Bob" finds a block "X"
    Then node "Alice" should reach block "X"
    And node "Alice" should reach a total liquidity info of "0" buy and "0" sell on unit "B"
    And node "Bob" should reach a total liquidity info of "0" buy and "0" sell on unit "B"
    And node "Custodian" should reach block "X"
    And node "Custodian" should reach a balance of "0" NBT

  Scenario: The network forks with multiple grants
    Given a network with nodes "1", "2" and "3" able to mint
    And a node "Random peer" only connected to node "3"

    And node "1" votes for grants "A", "B" and "C"
    And node "2" votes for grants "A", "B" and "C"
    And node "3" votes for grant "A"

    When node "3" disconnects from nodes "1" and "2"

    And node "1" finds blocks until custodian "A" is elected
    And node "1" finds 3 blocks
    And node "1" finds a block "1.1"

    And node "3" finds blocks until custodian "A" is elected

    And node "3" reconnects to node "1"
    And node "3" finds a block "3.1"
    Then node "1" should receive block "3.1" but stay at block "1.1"

    When node "1" finds a block "1.2"
    Then node "3" should reach block "1.2"
