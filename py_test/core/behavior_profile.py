# core/behavior_profile.py
# -*- coding: utf-8 -*-

from dataclasses import dataclass
from typing import Any, Dict, Optional

import config


@dataclass
class BehaviorDecision:
    ok: bool = True
    reason: str = "ok"
    delay_ack_sec: float = 0.0
    force_device_type: Optional[str] = None
    snapshot_reason: str = ""


class BehaviorProfile:
    def __init__(self, cfg: Optional[Dict[str, Any]] = None):
        self.cfg = cfg if isinstance(cfg, dict) else {}
        self.legacy = self.cfg.get("profile", "legacy") == "legacy"

    @classmethod
    def legacy_default(cls) -> "BehaviorProfile":
        return cls({"profile": "legacy"})

    @classmethod
    def from_config(cls, cfg: Optional[Dict[str, Any]]) -> "BehaviorProfile":
        return cls(cfg)

    def evaluate_add_device(self, port_id: str, slave_id: int, device_type: str, exists: bool) -> BehaviorDecision:
        rule = self._rule("add_device", slave_id)
        if rule:
            return self._decision_from_rule(rule)

        if self.legacy:
            if slave_id == 2:
                return BehaviorDecision(ok=False, reason="device_no_response")
            if slave_id == 3 and exists:
                return BehaviorDecision(ok=False, reason="device_exists")
            if slave_id == 4:
                return BehaviorDecision(ok=False, reason="port_not_found")
            if slave_id == 6:
                return BehaviorDecision(
                    ok=True,
                    reason="ok_after_5s",
                    delay_ack_sec=5.0,
                    snapshot_reason="add_device_delayed_ack",
                )
            if slave_id == 7:
                return BehaviorDecision(ok=True, reason="ok", force_device_type="relay")

        return BehaviorDecision(ok=True, reason="ok")

    def evaluate_remove_device(self, port_id: str, slave_id: int, exists: bool) -> BehaviorDecision:
        rule = self._rule("remove_device", slave_id)
        if rule:
            return self._decision_from_rule(rule)

        if self.legacy:
            if slave_id == 2:
                return BehaviorDecision(ok=False, reason="device_not_found")
            if slave_id == 5:
                return BehaviorDecision(
                    ok=True,
                    reason="ok_after_5s",
                    delay_ack_sec=5.0,
                    snapshot_reason="remove_device_delayed_ack",
                )

        return BehaviorDecision(ok=True, reason="ok")

    def evaluate_set_relay(
        self,
        port_id: str,
        slave_id: int,
        device_exists: bool,
        is_relay: bool,
        states_valid: bool,
    ) -> BehaviorDecision:
        if not device_exists:
            return BehaviorDecision(ok=False, reason="device_not_found")
        if not is_relay:
            return BehaviorDecision(ok=False, reason="not_relay_device")
        if not states_valid:
            return BehaviorDecision(ok=False, reason="invalid_relay_states")

        rule = self._rule("set_relay", slave_id)
        if rule:
            return self._decision_from_rule(rule, default_delay=self._default_set_relay_delay())

        return BehaviorDecision(ok=True, reason="ok", delay_ack_sec=self._default_set_relay_delay())

    def evaluate_set_threshold(self, port_id: str, slave_id: int, device_exists: bool) -> BehaviorDecision:
        if not device_exists:
            return BehaviorDecision(ok=False, reason="device_not_found")

        rule = self._rule("set_threshold", slave_id)
        if rule:
            return self._decision_from_rule(rule)

        return BehaviorDecision(ok=True, reason="ok")

    def _default_set_relay_delay(self) -> float:
        default = float(getattr(config, "SET_RELAY_ACK_DELAY_SEC", 0.0) or 0.0)
        section = self.cfg.get("set_relay")
        if isinstance(section, dict) and "ack_delay_sec" in section:
            try:
                return float(section["ack_delay_sec"])
            except Exception:
                return default
        return default if self.legacy else 0.0

    def _rule(self, section_name: str, slave_id: int) -> Dict[str, Any]:
        section = self.cfg.get(section_name)
        if not isinstance(section, dict):
            return {}
        rules = section.get("rules")
        if isinstance(rules, list):
            for rule in rules:
                if not isinstance(rule, dict):
                    continue
                try:
                    if int(rule.get("slave_id")) == int(slave_id):
                        return rule
                except Exception:
                    continue
        by_slave = section.get("by_slave_id")
        if isinstance(by_slave, dict):
            rule = by_slave.get(str(slave_id))
            if isinstance(rule, dict):
                return rule
        return {}

    def _decision_from_rule(self, rule: Dict[str, Any], default_delay: float = 0.0) -> BehaviorDecision:
        action = str(rule.get("action") or rule.get("result") or "").strip()
        ok = bool(rule.get("ok", True))
        reason = str(rule.get("reason") or ("ok" if ok else "failed"))

        if action in {"device_no_response", "port_not_found", "device_not_found", "not_relay_device", "invalid_relay_states"}:
            ok = False
            reason = action
        elif action in {"fail", "failed"}:
            ok = False
            reason = str(rule.get("reason") or "failed")
        elif action in {"success", "ok", "delay_ack", "force_device_type"}:
            ok = True
            reason = str(rule.get("reason") or "ok")

        try:
            delay = float(rule.get("delay_ack_sec", rule.get("ack_delay_sec", default_delay)) or 0.0)
        except Exception:
            delay = default_delay

        force_device_type = rule.get("force_device_type") or rule.get("device_type")
        if force_device_type is not None:
            force_device_type = str(force_device_type)

        return BehaviorDecision(
            ok=ok,
            reason=reason,
            delay_ack_sec=delay,
            force_device_type=force_device_type,
            snapshot_reason=str(rule.get("snapshot_reason") or ""),
        )
