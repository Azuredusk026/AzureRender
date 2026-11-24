"""Exercise source and mapped scene ordering for the startup audit."""

import unittest
from types import SimpleNamespace

from tools.npr_room.startup_chain_inventory import candidate_route, recovery_route, scene_owner


class StartupChainTests(unittest.TestCase):
    def test_preserves_direct_holder_references(self):
        records = {
            6: {"m_GameObject": {"fileID": 3}, "m_Script": {"guid": "script"}, "m_Enabled": 1,
                "settings": {"guid": "asset", "fileID": 11400000}},
            3: {"m_Name": "Holder", "m_IsActive": 1},
        }
        scene = SimpleNamespace(serialized="guid: script", documents={6: (114, 0, 12)}, read=records.__getitem__)
        report = scene_owner(scene, "script", ("settings",), {"asset": ["Assets/settings.asset"]})
        self.assertEqual(report["componentFileId"], 6)
        self.assertEqual(report["directReferences"]["settings"]["assetPath"], "Assets/settings.asset")
        self.assertEqual(report["directReferences"]["settings"]["fileId"], 11400000)
        with self.assertRaisesRegex(ValueError, "settings"):
            scene_owner(scene, "script", ("settings",), {})
    def test_distinguishes_transition_and_room_activation(self):
        result = recovery_route(
            'await Addressables.InitializeAsync(); await SceneLoader.LoadSceneAdditiveAsync("AssetLoad");'
            'await SceneLoader.LoadSceneAdditiveAsync("Entry");',
            'float startTime = Time.time; await LoadAsync(cancellation);'
            'var h = SceneLoader.LoadSceneAsync("RoomScene", autoActivate: false);'
            'await UniTask.WaitUntil(() => h.IsReady);'
            'float n = Time.time - startTime; float delay = Mathf.Max(0f, 2f - n);'
            'await UniTask.Delay(TimeSpan.FromSeconds(0.5));'
            'await UniTask.WaitUntil(() => loadDirectionService.IsFinishDirection(loadDirectionType)); h.Activate();',
            'await SceneManager.LoadSceneAsync("Transition");'
            '_mainOperation = SceneManager.LoadSceneAsync(sceneName);'
            '_mainOperation.allowSceneActivation = autoActivate;'
            'return _mainOperation.progress >= 0.9f; _mainOperation.allowSceneActivation = true;',
            'return SceneLoadHandle.Load(sceneName, autoActivate);'
            'await SceneManager.LoadSceneAsync(sceneName, LoadSceneMode.Additive);',
        )
        self.assertEqual([step["scene"] for step in result["steps"]],
                         ["Boot", "AssetLoad", "Entry", "Transition", "RoomScene"])
        self.assertEqual([step["mode"] for step in result["steps"]],
                         ["startup", "additive", "additive", "single", "manual-activation"])
        self.assertEqual(result["readyProgress"], 0.9)
        self.assertEqual(result["minimumSeconds"], 2.0)
        self.assertEqual(result["delayAfterRevealSeconds"], 0.5)

    def test_detects_changed_candidate_graph_and_paths(self):
        script = ('local room = "assets:/room.azurelevel"\n'
                  'local next_level = { startup = "assets:/asset-load.azurelevel",\n'
                  '["asset-load"] = "assets:/entry.azurelevel", entry = "assets:/transition.azurelevel", }\n'
                  'self:preload_level(room)\n'
                  'if loading_seconds < 2 or not self:level_ready(room) then end\n'
                  'if reveal_seconds >= 0.5 then self:load_level(room) end\n')
        result = candidate_route("assets:/boot.azurelevel", script)
        self.assertEqual(result["levels"][-2:], ["assets:/transition.azurelevel", "assets:/room.azurelevel"])
        self.assertEqual(result["minimumTransitionSeconds"], 2)
        self.assertEqual(result["readyDelaySeconds"], 0.5)
        with self.assertRaisesRegex(ValueError, "route"):
            candidate_route("assets:/boot.azurelevel", script.replace("entry = ", "wrong = "))
