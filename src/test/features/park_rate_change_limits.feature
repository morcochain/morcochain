Feature: Park rate increase and decrease are limited

  Scenario: Park rates are increased in protocol 0.5
    Given a network with nodes "Alice" and "Bob" able to mint
    And  node "Alice" votes for these NuBit park rates:
      | Duration | Annual percentage rate |
      | 6 months |                   10 % |
      | 1 year   |                    5 % |
    And node "Alice" finds enough blocks for a park rate vote to become the median

    When node "Alice" finds a block received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                    1 % |
      | 1 year   |                    1 % |

    When node "Alice" finds a block received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                    2 % |
      | 1 year   |                    2 % |

  Scenario: Park rates are increased in protocol 2.0
    Given a network with nodes "Alice" and "Bob" able to mint
    And the network is at protocol 2.0
    And  node "Alice" votes for these NuBit park rates:
      | Duration | Annual percentage rate |
      | 6 months |                   10 % |
      | 1 year   |                0.005 % |
    And node "Alice" finds enough blocks for a park rate vote to become the median
    And node "Alice" finds enough blocks for the voted park rate to become effective

    And node "Alice" finds a block received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                0.002 % |
      | 1 year   |                0.002 % |

    When node "Alice" finds a block received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                0.004 % |
      | 1 year   |                0.004 % |

    When node "Alice" finds a block received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                0.006 % |
      | 1 year   |                0.005 % |

  Scenario: Park rates are decreased in protocol 0.5
    Given a network with nodes "Alice" and "Bob" able to mint
    And  node "Alice" votes for these NuBit park rates:
      | Duration | Annual percentage rate |
      | 6 months |                    2 % |
      | 1 year   |                    3 % |
    And node "Alice" finds enough blocks for a park rate vote to become the median

    When node "Alice" finds 3 blocks received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                    2 % |
      | 1 year   |                    3 % |

    And  node "Alice" votes for these NuBit park rates:
      | Duration | Annual percentage rate |
      | 6 months |                    0 % |
      | 1 year   |                    1 % |
    And node "Alice" finds enough blocks for a park rate vote to become the median

    And node "Alice" finds a block received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                    0 % |
      | 1 year   |                    1 % |

  Scenario: Park rates are decreased in protocol 2.0
    Given a network with nodes "Alice" and "Bob" able to mint
    And the network is at protocol 2.0
    And  node "Alice" votes for these NuBit park rates:
      | Duration | Annual percentage rate |
      | 6 months |                0.012 % |
      | 1 year   |                0.012 % |
    And node "Alice" finds enough blocks for a park rate vote to become the median
    And node "Alice" finds enough blocks for the voted park rate to become effective

    When node "Alice" finds 6 blocks received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                0.012 % |
      | 1 year   |                0.012 % |

    When node "Alice" votes for these NuBit park rates:
      | Duration | Annual percentage rate |
      | 6 months |                    0 % |
      | 1 year   |                0.011 % |
    And node "Alice" finds enough blocks for the APR "0 %" to become the median on "6 months"
    And node "Alice" finds enough blocks for the voted park rate to become effective
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                0.008 % |
      | 1 year   |                0.011 % |

    When node "Alice" finds a block received by all other nodes
    Then the NuBit park rates should be
      | Duration | Annual percentage rate |
      | 6 months |                0.004 % |
      | 1 year   |                0.011 % |

