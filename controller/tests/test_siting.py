"""Tests for satisfactory_ai.siting (resource-node siting toolkit).

Synthetic nodes with round-number purities so extraction rates and cluster
membership are hand-verifiable. Run from controller/:
    python -m unittest tests.test_siting -v
"""

import unittest

from satisfactory_ai.models import Position, ResourceNode
from satisfactory_ai.siting import (
    NodeCluster,
    available_rate_by_resource,
    cluster_nodes,
    evaluate_site,
    extraction_rate,
    group_by_resource,
    rank_sites_for_demand,
)

IRON = "Desc_OreIron_C"
COPPER = "Desc_OreCopper_C"


def node(nid, rc, purity, x, y=0.0, z=0.0, occupied=False):
    return ResourceNode(id=nid, resource=rc.replace("Desc_Ore", "").replace("_C", ""),
                        resource_class=rc, purity=purity, position=Position(x=x, y=y, z=z),
                        occupied=occupied)


class SitingTest(unittest.TestCase):
    def setUp(self):
        # Cluster A near origin: iron Normal + iron Pure + copper Normal.
        # Cluster B far east: a single impure iron node.
        self.nodes = [
            node("A1", IRON, "Normal", 0.0),
            node("A2", IRON, "Pure", 1000.0),
            node("A3", COPPER, "Normal", 2000.0),
            node("B1", IRON, "Impure", 100000.0),
        ]

    def test_extraction_rate(self):
        self.assertEqual(extraction_rate("Normal", 1), 60.0)
        self.assertEqual(extraction_rate("Impure", 1), 30.0)
        self.assertEqual(extraction_rate("Pure", 1), 120.0)
        self.assertEqual(extraction_rate("Normal", 2), 120.0)   # Mk2 = 2x
        self.assertEqual(extraction_rate("Pure", 3), 480.0)      # Mk3 = 4x
        self.assertEqual(extraction_rate("Normal", 1, 50.0), 30.0)  # clock scales

    def test_group_by_resource(self):
        g = group_by_resource(self.nodes)
        self.assertEqual(len(g[IRON]), 3)
        self.assertEqual(len(g[COPPER]), 1)

    def test_available_rate_excludes_occupied(self):
        base = available_rate_by_resource(self.nodes, miner_mk=1)
        self.assertEqual(base[IRON], 60 + 120 + 30)   # Normal + Pure + Impure
        self.assertEqual(base[COPPER], 60)
        occ = [node("A1", IRON, "Normal", 0.0, occupied=True)] + self.nodes[1:]
        after = available_rate_by_resource(occ, miner_mk=1)
        self.assertEqual(after[IRON], 120 + 30)       # A1 excluded

    def test_clustering(self):
        clusters = cluster_nodes(self.nodes, link_radius=5000.0)
        self.assertEqual(len(clusters), 2)
        big = clusters[0]
        self.assertEqual(len(big.nodes), 3)               # A1,A2,A3
        self.assertCountEqual(big.resource_classes, [IRON, COPPER])
        self.assertEqual(len(clusters[1].nodes), 1)        # B1

    def test_evaluate_and_rank(self):
        demand = {IRON: 100.0, COPPER: 30.0}
        ranked = rank_sites_for_demand(self.nodes, demand, link_radius=5000.0, miner_mk=1)
        self.assertEqual(len(ranked), 2)
        best = ranked[0]
        self.assertTrue(best.satisfied)                    # A: iron 180>=100, copper 60>=30
        self.assertEqual(best.covered_count, 2)
        worst = ranked[1]
        self.assertFalse(worst.satisfied)                  # B: iron 30<100, no copper
        self.assertAlmostEqual(worst.deficits[IRON], 70.0, places=6)
        self.assertAlmostEqual(worst.deficits[COPPER], 30.0, places=6)

    def test_evaluate_site_direct(self):
        clusters = cluster_nodes(self.nodes, link_radius=5000.0)
        cov = evaluate_site(clusters[0], {IRON: 500.0}, miner_mk=1)
        self.assertFalse(cov.satisfied)                    # only 180 iron in cluster A
        self.assertAlmostEqual(cov.deficits[IRON], 320.0, places=6)
        cov2 = evaluate_site(clusters[0], {IRON: 500.0}, miner_mk=3)  # 4x -> 720
        self.assertTrue(cov2.satisfied)


if __name__ == "__main__":
    unittest.main()
