#!/usr/bin/env python3
"""Print non-secret config for unified decision control and direct generation policies."""
import argparse
import json


def policy(family=None):
    requirements = ['and', ['meets_req'], ['not', ['is', 'disabled']]]
    if family:
        requirements.append(['family_eq', family])
    return ['policy', requirements, ['neg', ['field', 'price_in']],
            ['top_k', 4, ['prefer', ['not', ['is', 'breaker_open']], ['argmax']]],
            ['id'], ['always', {'action': 'next_candidate'}]]


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--economy-family', required=True, help='Exact Luna (or alternative) family from router /v1/models')
    p.add_argument('--capable-family', required=True, help='Exact Astra (or alternative) family from router /v1/models')
    p.add_argument('--decision-family', help='Optional decision-model family; default accepts compatible decision models')
    a = p.parse_args()
    print('decision_extra = ' + json.dumps({'policy_ir': policy(a.decision_family)}, separators=(',', ':')))
    print('request_extra = ' + json.dumps({'policy_ir': policy(a.capable_family)}, separators=(',', ':')))
    print('economy_extra = ' + json.dumps({'policy_ir': policy(a.economy_family)}, separators=(',', ':')))
    print('decision_context_bytes = 18000')
