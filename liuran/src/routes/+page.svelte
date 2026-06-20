<script lang="ts">
	import bio from '$lib/assets/IMG_8559.jpg';
	import SocialLinks from '$lib/components/SocialLinks.svelte';

	const lastUpdated = new Date(__LAST_COMMIT_DATE__).toLocaleDateString('en-US', {
		year: 'numeric',
		month: 'long',
		day: 'numeric'
	});
</script>

<svelte:head>
	<title>Ran Liu</title>
</svelte:head>

<div class="about-container">
	<div class="about-left-container">
		<div
			class="portrait"
			style="width: var(--left-container-width); height: var(--left-container-width); border-radius: 50%;
            background-image: url({bio}); background-size: 130%; background-position: 80% 80%; background-repeat: no-repeat;"
			role="img"
			aria-label="Portrait"
		></div>
		<h1>Ran Liu</h1>
		<h3>Scientist and biomedical engineer</h3>
		<h4><a href="mailto:me@liuran.net">me@liuran.net</a></h4>
	</div>
	<main class="about-right-container">
		<h1>About</h1>
		<p>I'm joining Google as a Research Data Scientist, working on AI metrics.</p>
		<p>
			Previously, I worked at Apple as a Machine Learning Algorithms Engineer. I developed methods
			for sensor fusion, motion, touch, and force sensing, as well as haptics and audio rendering on
			iPad and Apple Pencil.
		</p>
		<p>
			I was a Postdoctoral Scholar with
			<a href="https://purdonlab.stanford.edu" target="_blank" rel="noopener noreferrer"
				>Patrick Purdon</a
			>
			in the department of Anesthesiology, Perioperative and Pain Medicine at Stanford University, and
			previously at Massachusetts General Hospital. My Postdoctoral research was funded in part by the
			NIH through an NIGMS NRSA fellowship (<a
				href="https://reporter.nih.gov/search/I7PVmY38_EGQRCTeSb2Yzg/project-details/10899776#description"
				target="_blank"
				rel="noopener noreferrer">F32GM148114</a
			>). I worked on causal inference and state space modeling for treatment optimization in
			anesthesia and pain management.
		</p>
		<p>
			I completed both my PhD (2021) and my BS (2021) in Biomedical Engineering at Johns Hopkins
			University.
		</p>
		<p style="color: var(--muted);">This page was last updated on {lastUpdated}.</p>
		<SocialLinks />
	</main>
</div>

<style>
	.about-container {
		display: flex;
		flex-direction: row;
		align-items: center;
		gap: var(--container-gap);
	}

	.about-left-container {
		display: flex;
		flex-direction: column;
		text-align: center;
		max-width: var(--left-container-width);
	}

	.about-left-container h1,
	h3,
	h4 {
		font-family: var(--font-display);
	}

	.about-left-container h3 {
		color: var(--muted);
	}

	.about-left-container h3,
	h4 {
		font-weight: 400;
	}

	.about-left-container a {
		color: var(--muted);
		text-decoration: none;
	}

	.about-left-container a:hover {
		text-decoration: underline;
		text-decoration-color: var(--muted);
	}

	.portrait {
		margin: 0 0 var(--space-8);
	}

	.about-right-container {
		display: flex;
		flex-direction: column;
		max-width: var(--right-container-width);
	}

	.about-right-container h1 {
		color: var(--accent);
	}

	@media only screen and (max-width: 1024px) {
		.about-container {
			flex-direction: column;
			gap: var(--space-4);
		}
	}
</style>
