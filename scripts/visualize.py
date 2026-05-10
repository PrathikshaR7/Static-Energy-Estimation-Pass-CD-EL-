#!/usr/bin/env python3
"""
visualize.py — EnergyPass HTML Report Generator
Phase 10: Visualization Script

Usage:
    python3 scripts/visualize.py --all
    python3 scripts/visualize.py \
        --remarks /tmp/01_integer_remarks.yml \
        --sources test/01_integer.c \
        --output  report.html
"""

import argparse
import os
import re
import sys
import tempfile
from collections import defaultdict

REPO    = os.path.expanduser('~/Static-Energy-Estimation-Pass-CD-EL-')
TMPDIR  = tempfile.gettempdir()
TESTNAMES = ['01_integer','02_float','03_memory',
             '04_nested_loops','05_mixed']

# ---------------------------------------------------------------------------
# YAML remark parser
# ---------------------------------------------------------------------------
def parse_remarks(yml_path):
    remarks = []
    if not os.path.exists(yml_path):
        return remarks
    with open(yml_path) as f:
        content = f.read()
    docs = re.split(r'^---', content, flags=re.MULTILINE)
    for doc in docs:
        doc = doc.strip()
        if not doc or doc == '...':
            continue
        remark = {}
        m = re.search(r'^Name:\s+(\S+)',     doc, re.MULTILINE)
        if m: remark['name'] = m.group(1)
        m = re.search(r'^Pass:\s+(\S+)',     doc, re.MULTILINE)
        if m: remark['pass'] = m.group(1)
        m = re.search(r'^Function:\s+(\S+)', doc, re.MULTILINE)
        if m: remark['function'] = m.group(1)
        m = re.search(
            r"DebugLoc:\s+\{\s*File:\s+'?([^',}]+)'?,\s*Line:\s+(\d+),\s*Column:\s+(\d+)",
            doc)
        if m:
            remark['file']   = m.group(1).strip()
            remark['line']   = int(m.group(2))
            remark['column'] = int(m.group(3))
        m = re.search(r"- String:\s+'(.+)'", doc, re.DOTALL)
        if m:
            remark['message'] = m.group(1).replace("''", "'")
        if 'name' in remark and 'function' in remark:
            remarks.append(remark)
    return remarks

def extract_float(message, key):
    m = re.search(rf'{key}=([\d.]+)', message)
    return float(m.group(1)) if m else 0.0

def extract_int(message, key):
    m = re.search(rf'{key}=(\d+)', message)
    return int(m.group(1)) if m else 0

def parse_function_remark(r):
    msg = r.get('message', '')
    arch_m = re.search(r'arch=(\S+)', msg)
    return {
        'function':        r.get('function', ''),
        'file':            r.get('file', ''),
        'line':            r.get('line', 0),
        'arch':            arch_m.group(1) if arch_m else '',
        'static_energy':   extract_float(msg, 'static-energy'),
        'weighted_energy': extract_float(msg, 'weighted-energy'),
        'instrs':          extract_int(msg,   'instrs'),
    }

def parse_hotblock_remark(r):
    msg = r.get('message', '')
    block_m = re.search(r"hot block '([^']+)'", msg)
    return {
        'block':           block_m.group(1) if block_m else '',
        'function':        r.get('function', ''),
        'file':            r.get('file', ''),
        'line':            r.get('line', 0),
        'loop_depth':      extract_int(msg,   'loop-depth'),
        'freq_weight':     extract_float(msg, 'freq-weight'),
        'static_energy':   extract_float(msg, 'static-energy'),
        'weighted_energy': extract_float(msg, 'weighted-energy'),
    }

# ---------------------------------------------------------------------------
# HTML generation
# ---------------------------------------------------------------------------
HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>EnergyPass Report</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
         background: #0f1117; color: #e2e8f0; line-height: 1.6; }
  h1 { padding: 2rem; color: #f8fafc; border-bottom: 1px solid #1e293b;
       font-size: 1.5rem; }
  h1 span { color: #38bdf8; }
  h2 { font-size: 1.1rem; color: #94a3b8; padding: 1.5rem 2rem 0.5rem;
       text-transform: uppercase; letter-spacing: 0.05em; }
  h3 { font-size: 1rem; color: #38bdf8; padding: 1rem 2rem 0.25rem; }
  .summary-table { width: calc(100% - 4rem); margin: 1rem 2rem;
                   border-collapse: collapse; font-size: 0.85rem; }
  .summary-table th { background: #1e293b; color: #94a3b8;
                      padding: 0.5rem 1rem; text-align: left; font-weight: 600; }
  .summary-table td { padding: 0.4rem 1rem; border-bottom: 1px solid #1e293b; }
  .summary-table tr:hover td { background: #1e293b; }
  .energy-high   { color: #f87171; font-weight: 600; }
  .energy-medium { color: #fbbf24; }
  .energy-low    { color: #34d399; }
  .bar-cell { width: 200px; }
  .bar-bg   { background: #1e293b; border-radius: 3px; height: 8px; }
  .bar-fill { height: 8px; border-radius: 3px;
              background: linear-gradient(90deg, #3b82f6, #06b6d4); }
  .source-file { margin: 0 2rem 2rem; border: 1px solid #1e293b;
                 border-radius: 8px; overflow: hidden; }
  .source-file-header { background: #1e293b; padding: 0.75rem 1rem;
                        font-size: 0.85rem; color: #94a3b8; font-family: monospace; }
  .source-table { width: 100%; border-collapse: collapse;
                  font-family: 'Fira Code', 'Cascadia Code', monospace;
                  font-size: 0.8rem; }
  .source-table td { padding: 1px 0; vertical-align: top; }
  .line-num  { color: #334155; padding: 0 1rem; text-align: right;
               min-width: 3rem; user-select: none;
               border-right: 1px solid #1e293b; }
  .line-code { padding: 0 1rem; white-space: pre; color: #cbd5e1; }
  .line-annot { padding: 0 0.5rem; min-width: 320px; }
  .hot-depth-1 .line-code { background: rgba(251,191,36,0.07); }
  .hot-depth-2 .line-code { background: rgba(248,113,113,0.10); }
  .hot-depth-3 .line-code { background: rgba(248,113,113,0.18); }
  .annot-pill     { display: inline-block; border-radius: 4px;
                    padding: 1px 8px; font-size: 0.72rem;
                    margin: 1px 0; white-space: nowrap; }
  .annot-function { background: #1e40af; color: #bfdbfe; }
  .annot-hotblock { background: #7c2d12; color: #fed7aa; }
  .annot-depth    { background: #134e4a; color: #99f6e4; margin-left: 4px; }
  .fn-grid { display: grid;
             grid-template-columns: repeat(auto-fill, minmax(340px,1fr));
             gap: 1rem; margin: 0.5rem 2rem 2rem; }
  .fn-card { background: #1e293b; border-radius: 8px; padding: 1rem; }
  .fn-card h4 { color: #f8fafc; font-size: 0.9rem; margin-bottom: 0.5rem; }
  .fn-card .meta { color: #64748b; font-size: 0.75rem; margin-bottom: 0.75rem; }
  .fn-stat { display: flex; justify-content: space-between;
             align-items: center; margin: 4px 0; font-size: 0.8rem; }
  .fn-stat .label { color: #94a3b8; }
  .fn-stat .value { font-weight: 600; color: #f8fafc; }
  .amplification  { color: #f87171; }
  .section-divider { border: none; border-top: 1px solid #1e293b;
                     margin: 1rem 2rem; }
</style>
</head>
<body>
<h1>⚡ <span>EnergyPass</span> Analysis Report</h1>
{BODY}
</body>
</html>
"""

def energy_class(val, max_val):
    if max_val == 0: return ''
    r = val / max_val
    if r > 0.6:  return 'energy-high'
    if r > 0.25: return 'energy-medium'
    return 'energy-low'

def html_escape(s):
    return (str(s).replace('&','&amp;').replace('<','&lt;')
                  .replace('>','&gt;').replace('"','&quot;'))

def make_bar(value, max_value, width=160):
    if max_value == 0: pct = 0
    else: pct = min(100, value / max_value * 100)
    fill = int(pct / 100 * width)
    return (f'<div class="bar-bg" style="width:{width}px">'
            f'<div class="bar-fill" style="width:{fill}px"></div></div>')

def build_summary_table(all_fn_remarks):
    if not all_fn_remarks: return ''
    max_w = max((r['weighted_energy'] for r in all_fn_remarks), default=1)
    rows  = []
    for r in sorted(all_fn_remarks, key=lambda x: -x['weighted_energy']):
        amp = (r['weighted_energy'] / r['static_energy']
               if r['static_energy'] > 0 else 1.0)
        ec  = energy_class(r['weighted_energy'], max_w)
        rows.append(f"""
        <tr>
          <td><code>{html_escape(r['function'])}</code></td>
          <td style="color:#64748b;font-size:0.8rem">
            {html_escape(os.path.basename(r['file']))}</td>
          <td>{r['instrs']}</td>
          <td class="{ec}">{r['static_energy']:.2f}</td>
          <td class="{ec}">{r['weighted_energy']:.2f}</td>
          <td class="bar-cell">{make_bar(r['weighted_energy'], max_w)}</td>
          <td class="amplification">{amp:.1f}x</td>
        </tr>""")
    return f"""
<h2>Function Summary</h2>
<table class="summary-table">
  <thead><tr>
    <th>Function</th><th>File</th><th>Instrs</th>
    <th>Static Energy</th><th>Weighted Energy</th>
    <th>Relative Cost</th><th>Amplification</th>
  </tr></thead>
  <tbody>{''.join(rows)}</tbody>
</table>"""

def build_function_cards(fn_remarks, hot_remarks):
    if not fn_remarks: return ''
    cards = []
    for r in sorted(fn_remarks, key=lambda x: -x['weighted_energy']):
        fn   = r['function']
        amp  = (r['weighted_energy'] / r['static_energy']
                if r['static_energy'] > 0 else 1.0)
        hot  = [h for h in hot_remarks if h['function'] == fn]
        hot_rows = ''
        for h in sorted(hot, key=lambda x: -x['weighted_energy']):
            hot_rows += f"""
          <div class="fn-stat">
            <span class="label">↳ {html_escape(h['block'])}
              <span style="color:#475569"> depth={h['loop_depth']}</span></span>
            <span class="value" style="color:#fbbf24">
              {h['weighted_energy']:.1f}</span>
          </div>"""
        cards.append(f"""
      <div class="fn-card">
        <h4>{html_escape(fn)}</h4>
        <div class="meta">arch={html_escape(r.get('arch',''))} &nbsp;
          instrs={r['instrs']} &nbsp;
          {html_escape(os.path.basename(r['file']))}</div>
        <div class="fn-stat">
          <span class="label">Static energy</span>
          <span class="value">{r['static_energy']:.4f}</span>
        </div>
        <div class="fn-stat">
          <span class="label">Weighted energy</span>
          <span class="value">{r['weighted_energy']:.4f}</span>
        </div>
        <div class="fn-stat">
          <span class="label">Loop amplification</span>
          <span class="value amplification">{amp:.1f}x</span>
        </div>
        {hot_rows}
      </div>""")
    return f'<h2>Function Details</h2><div class="fn-grid">{"".join(cards)}</div>'

def build_annotated_source(src_path, fn_remarks, hot_remarks):
    if not os.path.exists(src_path):
        return (f'<p style="color:#f87171;padding:1rem 2rem">'
                f'Source not found: {html_escape(src_path)}</p>')
    with open(src_path) as f:
        lines = f.readlines()
    annots   = defaultdict(list)
    hotlines = {}
    for r in fn_remarks:
        if r['line'] > 0:
            amp = (r['weighted_energy'] / r['static_energy']
                   if r['static_energy'] > 0 else 1.0)
            annots[r['line']].append(
                f'<span class="annot-pill annot-function">'
                f'⚡ {html_escape(r["function"])} '
                f'w={r["weighted_energy"]:.1f} ({amp:.1f}x)</span>')
    for h in hot_remarks:
        if h['line'] > 0:
            annots[h['line']].append(
                f'<span class="annot-pill annot-hotblock">'
                f'🔥 {html_escape(h["block"])} '
                f'w={h["weighted_energy"]:.1f}</span>'
                f'<span class="annot-pill annot-depth">'
                f'depth={h["loop_depth"]}</span>')
            hotlines[h['line']] = max(hotlines.get(h['line'], 0),
                                      h['loop_depth'])
    rows = []
    for i, line in enumerate(lines, 1):
        depth   = hotlines.get(i, 0)
        hot_cls = f' hot-depth-{min(depth,3)}' if depth > 0 else ''
        ann     = ' '.join(annots.get(i, []))
        rows.append(
            f'<tr class="{hot_cls}">'
            f'<td class="line-num">{i}</td>'
            f'<td class="line-code">{html_escape(line.rstrip())}</td>'
            f'<td class="line-annot">{ann}</td>'
            f'</tr>')
    return f"""
<div class="source-file">
  <div class="source-file-header">📄 {html_escape(src_path)}</div>
  <table class="source-table"><tbody>
    {''.join(rows)}
  </tbody></table>
</div>"""

# ---------------------------------------------------------------------------
# Core
# ---------------------------------------------------------------------------
def generate_report(remark_files, source_files, output_path):
    all_fn_remarks  = []
    all_hot_remarks = []
    src_sections    = []

    remark_map = {}
    for rf in remark_files:
        key = re.sub(r'_remarks\.yml$', '',
                     os.path.basename(rf).replace('\\', '/'))
        remark_map[key] = rf

    source_map = {}
    for sf in source_files:
        key = re.sub(r'\.c$', '', os.path.basename(sf))
        source_map[key] = sf

    for key in sorted(set(list(remark_map) + list(source_map))):
        rf = remark_map.get(key)
        sf = source_map.get(key)
        fn_remarks  = []
        hot_remarks = []
        if rf:
            for r in parse_remarks(rf):
                if r.get('name') == 'FunctionEnergy':
                    fn_remarks.append(parse_function_remark(r))
                elif r.get('name') == 'HotBlock':
                    hot_remarks.append(parse_hotblock_remark(r))
        all_fn_remarks.extend(fn_remarks)
        all_hot_remarks.extend(hot_remarks)
        if sf:
            src_sections.append(
                f'<h3>📄 {html_escape(sf)}</h3>' +
                build_annotated_source(sf, fn_remarks, hot_remarks))

    body = [
        build_summary_table(all_fn_remarks),
        '<hr class="section-divider">',
        build_function_cards(all_fn_remarks, all_hot_remarks),
        '<hr class="section-divider">',
        '<h2>Annotated Source</h2>',
    ] + src_sections

    html = HTML_TEMPLATE.replace('{BODY}', '\n'.join(body))
    with open(output_path, 'w') as f:
        f.write(html)
    print(f'Report written to: {output_path}')

def main():
    parser = argparse.ArgumentParser(
        description='Generate HTML energy report from LLVM remark YAML files')
    parser.add_argument('--remarks', nargs='+', metavar='YAML')
    parser.add_argument('--sources', nargs='+', metavar='C')
    parser.add_argument('--output',  default='report.html')
    parser.add_argument('--all',     action='store_true',
                        help='Run all standard test cases automatically')
    args = parser.parse_args()

    if args.all:
        remark_files = [
            os.path.join(TMPDIR, f'{k}_remarks.yml')
            for k in TESTNAMES
        ]
        source_files = [
            os.path.join(REPO, 'test', f'{k}.c')
            for k in TESTNAMES
        ]
        output = os.path.join(REPO, 'report.html')
        generate_report(remark_files, source_files, output)
    else:
        if not args.remarks or not args.sources:
            parser.error('Provide --remarks and --sources, or use --all')
        generate_report(args.remarks, args.sources, args.output)

if __name__ == '__main__':
    main()
