<script lang="ts">
	import { resolve } from '$app/paths';
</script>

<svelte:head>
	<title>Ran Liu - Software</title>
</svelte:head>

<main>
	<h1>Open-source contributions</h1>
	<ul class="software">
		<li>
			<p class="title">
				<a href="https://github.com/mh105/somata" target="_blank" rel="noopener noreferrer"
					>SOMATA: State-space oscillator modeling and time series analysis</a
				>
			</p>
			<p class="description">
				State-space Oscillator Modeling And Time-series Analysis (SOMATA) is a Python library for
				state-space neural signal processing algorithms developed in the Purdon Lab.
			</p>
			<p class="authors">Authors: Mingjian He, Proloy Das, Ran Liu, Amanda Beck, Patrick Purdon</p>
		</li>
	</ul>

	<h1>Toy projects</h1>
	<ul class="software">
		<li>
			<p class="title">
				<a href={resolve('/software/pushfold/')}>Push/fold heads-up Texas Hold'em solver</a>
			</p>
			<p class="description">
				A Nash equilibrium solver for the 2-player Push/Fold (all-in or fold preflop) subgame of
				Texas Hold'em, written in Rust and compiled to WebAssembly. The game is solved via <a
					href="https://arxiv.org/abs/1407.5042"
					target="_blank"
					rel="noopener noreferrer">CFR+</a
				>, a counterfactual regret minimization algorithm variant with good empirical performance.
				Precomputed tables of equities and matchup probabilities enable fast solutions.
			</p>
		</li>
	</ul>
</main>

<style>
	.software {
		list-style: none;
		margin: 0 0 var(--space-8);
		padding: 0;
		display: flex;
		flex-direction: column;
		gap: var(--space-6);
	}

	.title {
		font-family: var(--font-serif);
		font-weight: 600;
		text-align: left;
		margin: 0 0 var(--space-1);
	}

	.authors,
	.description {
		text-align: left;
		margin: 0 0 var(--space-1);
	}

	.authors {
		color: var(--muted);
	}
</style>
