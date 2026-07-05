package telemetry

import (
	"fmt"
	"strings"
	"testing"
)

func TestHumanBytesFormatsBinaryBoundaries(t *testing.T) {
	tests := []struct {
		value int64
		want  string
	}{
		{value: 1073741824, want: "1.0 GiB"},
		{value: 1048576, want: "1.0 MiB"},
		{value: 0, want: "0 B"},
	}

	for _, tt := range tests {
		// R-FO9Q-65IH
		if got := humanBytes(tt.value); got != tt.want {
			t.Fatalf("humanBytes(%d) = %q, want %q", tt.value, got, tt.want)
		}
	}
}

func TestHeroChartMapsCapacityToPlotBounds(t *testing.T) {
	points := heroPointGeometry([]Sample{{Value: 1024}, {Value: 0}}, 1024)

	// R-FPHM-JX96
	if points[0].Y != heroPlotTop {
		t.Fatalf("sample equal to total y = %.1f, want plot top %.1f", points[0].Y, heroPlotTop)
	}
	if points[1].Y != heroPlotBottom {
		t.Fatalf("zero sample y = %.1f, want baseline %.1f", points[1].Y, heroPlotBottom)
	}

	markup := string(heroChart("free memory", []Sample{{Value: 1024}, {Value: 0}}, 1024))
	if !strings.Contains(markup, fmt.Sprintf(`cy="%s"`, coord(heroPlotTop))) {
		t.Fatalf("hero chart does not render top y-coordinate %s: %s", coord(heroPlotTop), markup)
	}
	if !strings.Contains(markup, fmt.Sprintf(`cy="%s"`, coord(heroPlotBottom))) {
		t.Fatalf("hero chart does not render baseline y-coordinate %s: %s", coord(heroPlotBottom), markup)
	}
}

func TestHeroChartRendersLatestCurrentValue(t *testing.T) {
	samples := []Sample{{Value: 1048576}, {Value: 1073741824}}
	markup := string(heroChart("free disk", samples, 2*1073741824))
	want := `<text class="chart-current" x="40" y="56">1.0 GiB</text>`

	// R-FQPI-XOZV
	if !strings.Contains(markup, want) {
		t.Fatalf("current-value label missing %q in %s", want, markup)
	}
}

func TestStackedChartFoldsBeyondTopSevenIntoOther(t *testing.T) {
	series := byteSeries(8)
	bands := stackedBandGeometry(series, orderedNames(8))
	names := bandNames(bands)

	// R-FRXF-BGQK
	if len(names) != 8 {
		t.Fatalf("band count with more than seven services = %d, want 8", len(names))
	}
	if names[len(names)-1] != "Other" {
		t.Fatalf("last band with more than seven services = %q, want Other; all bands %v", names[len(names)-1], names)
	}
	if containsName(names, "svc-01") {
		t.Fatalf("smallest service should be folded into Other, got bands %v", names)
	}

	withoutOther := bandNames(stackedBandGeometry(byteSeries(7), orderedNames(7)))
	// R-FRXF-BGQK
	if containsName(withoutOther, "Other") {
		t.Fatalf("Other band rendered at threshold of seven services: %v", withoutOther)
	}
}

func TestStackedChartGeometryAccumulatesTotals(t *testing.T) {
	series := map[string][]Sample{
		"alpha":   {{Value: 1}, {Value: 2}},
		"bravo":   {{Value: 3}, {Value: 4}},
		"charlie": {{Value: 5}, {Value: 6}},
	}
	bands := stackedBandGeometry(series, []string{"alpha", "bravo", "charlie"})
	topBand := bands[len(bands)-1]

	// R-FT5B-P8H9
	if got, want := topBand.StackTop, []int64{9, 12}; !equalInt64s(got, want) {
		t.Fatalf("top accumulated totals = %v, want %v", got, want)
	}
	if topBand.Top[1].Y != stackedPlotTop {
		t.Fatalf("max accumulated point y = %.1f, want chart top %.1f", topBand.Top[1].Y, stackedPlotTop)
	}
}

func TestStackedChartLegendNamesVisibleBands(t *testing.T) {
	markup := string(stackedChart("service memory", byteSeries(8), orderedNames(8)))

	// R-FUD8-307Y
	for _, name := range []string{"svc-08", "svc-07", "svc-06", "svc-05", "svc-04", "svc-03", "svc-02", "Other"} {
		if !strings.Contains(markup, ">"+name+"</text>") {
			t.Fatalf("legend is missing %q in %s", name, markup)
		}
	}
}

func TestStackedChartUsesDistinctPaletteColors(t *testing.T) {
	bands := stackedBandGeometry(byteSeries(8), orderedNames(8))
	seen := make(map[string]string, len(bands))

	for i, band := range bands {
		// R-FVL4-GRYN
		if band.Color != bandPalette[i] {
			t.Fatalf("band %q color = %q, want palette color %q", band.Name, band.Color, bandPalette[i])
		}
		if previous, ok := seen[band.Color]; ok {
			t.Fatalf("bands %q and %q share color %q", previous, band.Name, band.Color)
		}
		seen[band.Color] = band.Name
	}

	markup := string(stackedChart("service disk", byteSeries(8), orderedNames(8)))
	for _, color := range bandPalette {
		if !strings.Contains(markup, `fill="`+color+`"`) {
			t.Fatalf("rendered chart does not use palette color %s: %s", color, markup)
		}
	}
}

func byteSeries(count int) map[string][]Sample {
	series := make(map[string][]Sample, count)
	for i := 1; i <= count; i++ {
		name := fmt.Sprintf("svc-%02d", i)
		series[name] = []Sample{{Value: int64(i)}, {Value: int64(i * 1024)}}
	}
	return series
}

func orderedNames(count int) []string {
	names := make([]string, count)
	for i := 1; i <= count; i++ {
		names[i-1] = fmt.Sprintf("svc-%02d", i)
	}
	return names
}

func bandNames(bands []stackedBand) []string {
	names := make([]string, len(bands))
	for i, band := range bands {
		names[i] = band.Name
	}
	return names
}

func containsName(names []string, want string) bool {
	for _, name := range names {
		if name == want {
			return true
		}
	}
	return false
}

func equalInt64s(a, b []int64) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}
