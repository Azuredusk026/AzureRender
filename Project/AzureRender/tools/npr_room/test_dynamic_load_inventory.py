import unittest

from dynamic_load_inventory import addressable_targets, image_keys, resource_targets, source_calls


class DynamicLoadTests(unittest.TestCase):
    def test_catalog_preserves_the_actual_requested_asset_type(self):
        catalog = {"keys": [{"value": "News/one.png", "locations": [0, 1]}], "locations": [
            {"resourceType": {"m_ClassName": "UnityEngine.Texture2D"}},
            {"resourceType": {"m_ClassName": "UnityEngine.Sprite"}},
        ]}
        self.assertEqual(addressable_targets(catalog, "News/one.png", "Sprite"), [1])
        self.assertEqual(addressable_targets(catalog, "News/one.png", "AudioClip"), [])
        self.assertEqual(image_keys("![](News/one.png) ![](News/one.png)"), ["News/one.png"])

    def test_classifies_call_expressions_and_resource_path_spelling(self):
        calls = source_calls(
            'Addressables.InitializeAsync();\nAddressables.LoadAssetAsync<Sprite>(url);\n'
            'Resources.Load<WebEnvProfile>("WebEnv/" + kind.ToName());\n'
            'Resources.Load<Config>("WebEnv/WebEnvConfig");\n'
        )
        self.assertEqual([(row["api"], row["keyExpression"]) for row in calls],
                         [("Addressables.InitializeAsync", ""), ("Addressables.LoadAssetAsync", "url"),
                          ("Resources.Load", '"WebEnv/" + kind.ToName()'),
                          ("Resources.Load", '"WebEnv/WebEnvConfig"')])
        self.assertEqual(resource_targets({"webenv/Local": ["Assets/Resources/webenv/Local.asset"]},
                                          "WebEnv/Local"), ["Assets/Resources/webenv/Local.asset"])
