// Generates (or verifies) the self-hosted Noto Serif SC subset.
//
// The site uses Noto Serif SC only for short parenthetical glosses inside
// English prose (essay titles, quotations, proper nouns), which currently
// amounts to a few dozen distinct characters. Shipping the full family would
// cost ~87MB unpacked; subsetting to the characters we actually use costs
// ~20KB, so we fetch a subset from the Google Fonts CSS API and commit both
// the resulting .woff2 and a manifest of the characters it covers.
//
//     pnpm fonts:cjk           regenerate the subset (needs network)
//     pnpm fonts:cjk:check     verify the committed subset is current (offline)
//
// Generation is deliberately NOT part of `nx build`: it reaches out to
// fonts.googleapis.com, so wiring it into the build would make builds fail
// offline and produce spurious diffs whenever Google reships the font. The
// build depends on the offline --check instead, which catches the case that
// actually matters — Chinese text added without regenerating the font.
//
// The @font-face rule lives in src/lib/styles/fonts.css and is hand-written;
// it declares a broad CJK unicode-range, so any character missing from the
// subset falls through to the reader's system CJK font rather than tofu.

import { readFile, readdir, writeFile, access } from 'node:fs/promises';
import { join, dirname, relative } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const SRC = join(ROOT, 'src');
const FONT = join(SRC, 'lib/fonts/noto-serif-sc-subset.woff2');
const MANIFEST = join(SRC, 'lib/fonts/noto-serif-sc-subset.txt');

// Blocks that Noto Serif SC is responsible for in our font stacks: unified
// ideographs (+ Ext A), CJK punctuation, compatibility ideographs, and the
// fullwidth forms (e.g. U+FF0C, the fullwidth comma). Keep in sync with the
// unicode-range in src/lib/styles/fonts.css.
const CJK_BLOCKS = [
	[0x3000, 0x303f],
	[0x3400, 0x4dbf],
	[0x4e00, 0x9fff],
	[0xf900, 0xfaff],
	[0xff00, 0xffef]
];

const isCjk = (cp) => CJK_BLOCKS.some(([lo, hi]) => cp >= lo && cp <= hi);

// Only text files can contribute glyphs; skip binaries and generated output.
const TEXT_FILE = /\.(md|svelte|ts|js|json|css|html|ya?ml)$/;
const SKIP_DIR = /^(generated|pkg|fonts|node_modules)$/;

async function* walk(dir) {
	for (const entry of await readdir(dir, { withFileTypes: true })) {
		if (entry.isDirectory()) {
			if (!SKIP_DIR.test(entry.name)) yield* walk(join(dir, entry.name));
		} else if (TEXT_FILE.test(entry.name)) {
			yield join(dir, entry.name);
		}
	}
}

/** Every distinct CJK character used anywhere under src/, sorted. */
async function scanChars() {
	const chars = new Set();
	for await (const file of walk(SRC)) {
		for (const ch of await readFile(file, 'utf8')) {
			if (isCjk(ch.codePointAt(0))) chars.add(ch);
		}
	}
	return [...chars].sort().join('');
}

async function check() {
	const wanted = await scanChars();

	try {
		await access(FONT);
	} catch {
		console.error(`Missing ${relative(ROOT, FONT)} — run \`pnpm fonts:cjk\`.`);
		process.exit(1);
	}

	let have = '';
	try {
		have = (await readFile(MANIFEST, 'utf8')).trim();
	} catch {
		console.error(`Missing ${relative(ROOT, MANIFEST)} — run \`pnpm fonts:cjk\`.`);
		process.exit(1);
	}

	const haveSet = new Set(have);
	const missing = [...wanted].filter((c) => !haveSet.has(c));

	if (missing.length > 0) {
		console.error(
			`The committed Noto Serif SC subset is missing ${missing.length} character(s) ` +
				`now used on the site: ${missing.join('')}\n` +
				`These would render in the reader's system CJK font instead of Noto Serif SC.\n` +
				`Run \`pnpm fonts:cjk\` and commit the result.`
		);
		process.exit(1);
	}

	const stale = [...haveSet].filter((c) => !wanted.includes(c));
	const note = stale.length > 0 ? ` (${stale.length} no longer used, harmless)` : '';
	console.log(`Noto Serif SC subset covers all ${[...wanted].length} characters in use${note}.`);
}

async function generate() {
	const text = await scanChars();

	if (text.length === 0) {
		console.error('No CJK characters found under src/ — refusing to write an empty subset.');
		process.exit(1);
	}
	console.log(`Found ${[...text].length} distinct CJK characters: ${text}`);

	// A browser-like UA is required, otherwise the API serves .ttf instead of .woff2.
	const UA =
		'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 ' +
		'(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36';

	const api = new URL('https://fonts.googleapis.com/css2');
	api.searchParams.set('family', 'Noto Serif SC:wght@200..900');
	api.searchParams.set('text', text);

	const cssRes = await fetch(api, { headers: { 'User-Agent': UA } });
	if (!cssRes.ok) throw new Error(`Font CSS request failed: ${cssRes.status} ${cssRes.statusText}`);
	const css = await cssRes.text();

	const urls = [...new Set([...css.matchAll(/url\((https:\/\/[^)]+)\)/g)].map((m) => m[1]))];
	if (urls.length !== 1) {
		throw new Error(
			`Expected exactly one font file for the subset, got ${urls.length}. ` +
				`The API response may have changed shape:\n${css}`
		);
	}

	const fontRes = await fetch(urls[0], { headers: { 'User-Agent': UA } });
	if (!fontRes.ok) throw new Error(`Font download failed: ${fontRes.status} ${fontRes.statusText}`);
	const font = Buffer.from(await fontRes.arrayBuffer());

	await writeFile(FONT, font);
	await writeFile(MANIFEST, text + '\n');
	console.log(`Wrote ${relative(ROOT, FONT)} (${(font.length / 1024).toFixed(1)} KB)`);
}

await (process.argv.includes('--check') ? check() : generate());
