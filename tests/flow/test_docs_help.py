from common import *


def _kv_list_to_dict(kv_list):
    if isinstance(kv_list, dict):
        return kv_list
    if not isinstance(kv_list, (list, tuple)):
        return {}
    res = {}
    for i in range(0, len(kv_list) - 1, 2):
        res[kv_list[i]] = kv_list[i + 1]
    return res


def _extract_docs_for_command(res, cmd_name):
    # Accept multiple response layouts across redis versions / clients
    names = {cmd_name, cmd_name.upper()}
    # Dict layout: { 'cf.reserve': { ... } }
    if isinstance(res, dict):
        for name in list(names) | {cmd_name.lower()} | {cmd_name.title()}:
            if name in res:
                return _kv_list_to_dict(res[name])
        # Some clients may key by bytes
        for name in list(names):
            bname = name.encode()
            if bname in res:
                return _kv_list_to_dict(res[bname])
        # Fallback: value may itself be the docs map
        return _kv_list_to_dict(res)

    # List layout options:
    # 1) [[name, [k, v, k, v, ...]], ...]
    # 2) [name, [k, v, k, v, ...]] for a single-command response
    if isinstance(res, (list, tuple)):
        # Single-command flat layout
        if len(res) == 2 and isinstance(res[0], (bytes, str)):
            name_cmp = res[0].decode() if isinstance(res[0], bytes) else res[0]
            if name_cmp in {cmd_name, cmd_name.upper(), cmd_name.lower(), cmd_name.title()}:
                return _kv_list_to_dict(res[1])
        for entry in res:
            if isinstance(entry, (list, tuple)) and len(entry) >= 2:
                name = entry[0]
                if isinstance(name, bytes):
                    name_cmp = name.decode()
                else:
                    name_cmp = name
                if name_cmp in names:
                    return _kv_list_to_dict(entry[1])
    return {}


def _extract_help_lines(res):
    # HELP typically returns a flat list of strings; be resilient to nesting
    def ensure_str(x):
        return x.decode() if isinstance(x, bytes) else x
    lines = []
    if isinstance(res, (list, tuple)):
        # Flatten one level if needed
        if res and isinstance(res[0], (list, tuple)):
            for x in res[0]:
                if isinstance(x, (bytes, str)):
                    lines.append(ensure_str(x))
        else:
            for x in res:
                if isinstance(x, (bytes, str)):
                    lines.append(ensure_str(x))
    elif isinstance(res, dict):
        # Rare, but handle map-like
        for v in res.values():
            if isinstance(v, (list, tuple)):
                for x in v:
                    if isinstance(x, (bytes, str)):
                        lines.append(ensure_str(x))
    return lines


class testCommandDocsAndHelp():
    def __init__(self):
        self.env = Env(decodeResponses=True)

    def test_command_docs_cf_reserve(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()

        res = env.cmd('COMMAND DOCS cf.reserve')
        docs = _extract_docs_for_command(res, 'cf.reserve')
        print(docs)
        # Verify core fields we set via RedisModule_SetCommandInfo
        assert 'summary' in docs, f"Missing 'summary' in COMMAND DOCS response: {docs}"
        assert 'complexity' in docs, f"Missing 'complexity' in COMMAND DOCS response: {docs}"
        env.assertEqual(docs['summary'], 'Creates a new Cuckoo Filter')
        env.assertEqual(docs['complexity'], 'O(1)')

        # Optional numeric arity
        if 'arity' in docs:
            env.assertEqual(docs['arity'], -3)

        # Verify additional fields when present
        if 'since' in docs:
            env.assertEqual(docs['since'], '1.0.0')
        if 'group' in docs:
            env.assertEqual(docs['group'], 'module')
        if 'module' in docs:
            assert isinstance(docs['module'], (str, bytes))
        if 'history' in docs:
            assert isinstance(docs['history'], (list, tuple))

        # Verify arguments list fully
        args_val = docs.get('arguments') or docs.get('args')
        assert isinstance(args_val, (list, tuple)), f"Missing 'arguments' in docs or wrong type: {docs}"
        norm_args = [_kv_list_to_dict(entry) for entry in args_val]

        expected_arg_names = ['key', 'capacity', 'bucketsize', 'maxiterations', 'expansion']
        got_names = [a.get('name') for a in norm_args]
        env.assertEqual(got_names, expected_arg_names)

        # Key arg should reference first key spec
        ks_idx = norm_args[0].get('key_spec_index')
        if ks_idx is not None:
            try:
                ks_idx = int(ks_idx)
            except Exception:
                pass
            env.assertEqual(ks_idx, 0)

        # Types
        first_type = norm_args[0].get('type')
        if isinstance(first_type, bytes):
            first_type = first_type.decode()
        assert str(first_type).lower() == 'key'
        for i in range(1, len(norm_args)):
            t = norm_args[i].get('type')
            if t is None:
                # Some servers/clients may omit 'type' for module args
                continue
            if isinstance(t, bytes):
                t = t.decode()
            t_str = str(t).lower()
            # Only ensure it's not 'key' (first arg already validated as key)
            assert t_str != 'key'

        # Optional flags for trailing args
        for name in ['bucketsize', 'maxiterations', 'expansion']:
            arg = next(a for a in norm_args if a.get('name') == name)
            flags = arg.get('flags')
            if isinstance(flags, (list, tuple)):
                present = any((isinstance(x, (str, bytes)) and (x.decode().lower() if isinstance(x, bytes) else x.lower()) == 'optional') for x in flags)
                assert present, f"Missing 'optional' flag for argument {name}: {arg}"
            elif isinstance(flags, (str, bytes)):
                fl = flags.decode() if isinstance(flags, bytes) else flags
                assert 'optional' in fl.lower(), f"Missing 'optional' flag for argument {name}: {arg}"
            else:
                # Some servers may omit flags; treat as failure to ensure presence
                raise AssertionError(f"Missing 'optional' flag for argument {name}: {arg}")

        # Key specs (optional in some clients' formatting)
        ks_val = (docs.get('key specifications') or docs.get('key-specifications') or
                  docs.get('key_specs') or docs.get('key-specs'))
        if ks_val is not None:
            assert isinstance(ks_val, (list, tuple)) and len(ks_val) >= 1
            first_ks = _kv_list_to_dict(ks_val[0])
            # begin_search may be a nested map
            bs = first_ks.get('begin_search') or first_ks.get('begin-search')
            if bs is not None:
                bs_map = _kv_list_to_dict(bs)
                idx = bs_map.get('index') or bs_map.get('by')
                if idx is not None:
                    idx_map = _kv_list_to_dict(idx)
                    pos = idx_map.get('pos') or idx_map.get('position')
                    if isinstance(pos, (bytes, str)) and not isinstance(pos, int):
                        try:
                            pos = int(pos.decode() if isinstance(pos, bytes) else pos)
                        except Exception:
                            pass
                    if isinstance(pos, int):
                        env.assertEqual(pos, 1)
