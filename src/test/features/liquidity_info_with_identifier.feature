Feature: Sending liquidity info with an identifier

  Scenario: Liquidity info with indentifier on protocol V05
    Given a network with nodes "Alice" and "Custodian" able to mint
    When node "Custodian" generates a NuBit address "cust"
    And node "Alice" votes an amount of "1,000,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And all nodes reach the same height
    And node "Custodian" sends a liquidity of "1000" buy and "2000" sell on unit "B" from address "cust" with identifier "1:"
    Then node "Alice" should reach a total liquidity info of "1000" buy and "2000" sell on unit "B"
    And 1 second passes
    And node "Custodian" sends a liquidity of "10" buy and "5" sell on unit "B" from address "cust" with identifier "2:"
    Then node "Alice" should reach a total liquidity info of "1010" buy and "2005" sell on unit "B"

    When node "Custodian" restarts
    And 1 second passes
    And node "Custodian" sends a liquidity of "1" buy and "2" sell on unit "B" from address "cust" with identifier "1:"
    Then node "Alice" should reach a total liquidity info of "1" buy and "2" sell on unit "B"

  Scenario: Liquidity info with indentifier on protocol 2.0
    Given a network with nodes "Alice" and "Custodian" able to mint
    And the network is at protocol 2.0
    When node "Custodian" generates a NuBit address "cust"
    And node "Alice" votes an amount of "1,000,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And all nodes reach the same height
    And node "Custodian" sends a liquidity of "1000" buy and "2000" sell on unit "B" from address "cust" with identifier "1:"
    Then node "Alice" should reach a total liquidity info of "1000" buy and "2000" sell on unit "B"
    And 1 second passes
    And node "Custodian" sends a liquidity of "10" buy and "5" sell on unit "B" from address "cust" with identifier "2:"
    Then node "Alice" should reach a total liquidity info of "1010" buy and "2005" sell on unit "B"

    When node "Custodian" restarts
    And 1 second passes
    And node "Custodian" sends a liquidity of "1" buy and "2" sell on unit "B" from address "cust" with identifier "1:"
    Then node "Alice" should reach a total liquidity info of "11" buy and "7" sell on unit "B"

  Scenario: Sending invalid liquidity info
    Given a network with nodes "Alice" and "Custodian" able to mint
    And the network is at protocol 2.0
    When node "Custodian" generates a NuBit address "cust"
    And node "Alice" votes an amount of "1,000,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And all nodes reach the same height
    Then sending from node "Custodian" a liquidity of "100" buy and "200" sell on unit "B" from address "cust" with identifier "✓" should fail

  Scenario: Sending too many liquidity info
    Given a network with nodes "Alice" and "Custodian" able to mint
    And the maximum identifier per custodian is 5
    And the network is at protocol 2.0
    When node "Custodian" generates a NuBit address "cust"
    And node "Alice" votes an amount of "1,000,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And all nodes reach the same height
    And node "Custodian" sends these liquidity info from address "cust" on unit "B" at 1 second interval:
      | Buy | Sell | Identifier |
      |   1 |    1 | 1          |
      |   1 |    1 | 2          |
      |   1 |    1 | 3          |
      |   1 |    1 | 4          |
      |   1 |    1 | 5          |
    Then node "Alice" should have these liquidity info from custodian "cust" on unit "B":
      | Buy | Sell | Identifier |
      |   1 |    1 | 1          |
      |   1 |    1 | 2          |
      |   1 |    1 | 3          |
      |   1 |    1 | 4          |
      |   1 |    1 | 5          |
    And node "Custodian" sends these liquidity info from address "cust" on unit "B" at 1 second interval:
      | Buy | Sell | Identifier |
      |  10 |    1 | 1          |
      |   1 |    1 | 6          |
    Then node "Alice" should have these liquidity info from custodian "cust" on unit "B":
      | Buy | Sell | Identifier |
      |  10 |    1 | 1          |
      |   1 |    1 | 3          |
      |   1 |    1 | 4          |
      |   1 |    1 | 5          |
      |   1 |    1 | 6          |
