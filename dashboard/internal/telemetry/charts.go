package telemetry

import (
	"fmt"
	"html"
	"html/template"
	"math"
	"sort"
	"strings"
)

const (
	chartWidth  = 640.0
	chartHeight = 240.0

	heroPlotLeft   = 40.0
	heroPlotTop    = 72.0
	heroPlotRight  = 624.0
	heroPlotBottom = 204.0

	stackedPlotLeft   = 40.0
	stackedPlotTop    = 28.0
	stackedPlotRight  = 624.0
	stackedPlotBottom = 172.0

	stackedTopBands = 7
)

var bandPalette = []string{
	"#0072B2",
	"#E69F00",
	"#009E73",
	"#CC79A7",
	"#56B4E9",
	"#D55E00",
	"#F0E442",
	"#999999",
}

type chartPoint struct {
	X     float64
	Y     float64
	Value int64
}

type stackedBand struct {
	Name     string
	Color    string
	Values   []int64
	StackTop []int64
	Top      []chartPoint
	Bottom   []chartPoint
}

type orderedSeries struct {
	name   string
	values []int64
	latest int64
}

func humanBytes(n int64) string {
	if n == 0 {
		return "0 B"
	}

	sign := ""
	value := float64(n)
	if n < 0 {
		sign = "-"
		value = math.Abs(value)
	}
	if value < 1024 {
		return fmt.Sprintf("%s%d B", sign, int64(value))
	}

	units := []string{"KiB", "MiB", "GiB", "TiB", "PiB"}
	for _, unit := range units {
		value /= 1024
		if value < 1024 || unit == units[len(units)-1] {
			return fmt.Sprintf("%s%.1f %s", sign, value, unit)
		}
	}
	return fmt.Sprintf("%s%.1f PiB", sign, value)
}

func heroChart(title string, samples []Sample, total int64) template.HTML {
	points := heroPointGeometry(samples, total)
	current := int64(0)
	if len(samples) > 0 {
		current = samples[len(samples)-1].Value
	}

	var b strings.Builder
	fmt.Fprintf(&b, `<svg class="telemetry-chart telemetry-hero-chart" viewBox="0 0 %.0f %.0f" role="img" aria-label="%s">`, chartWidth, chartHeight, escaped(title+" chart"))
	fmt.Fprintf(&b, `<title>%s</title>`, escaped(title))
	fmt.Fprintf(&b, `<text class="chart-heading" x="40" y="28">%s</text>`, escaped(title))
	fmt.Fprintf(&b, `<text class="chart-current" x="40" y="56">%s</text>`, escaped(humanBytes(current)))
	fmt.Fprintf(&b, `<line class="chart-axis" x1="%s" y1="%s" x2="%s" y2="%s" stroke="var(--cds-border-subtle-01, #c6c6c6)" />`, coord(heroPlotLeft), coord(heroPlotBottom), coord(heroPlotRight), coord(heroPlotBottom))
	fmt.Fprintf(&b, `<polyline class="chart-line" fill="none" stroke="var(--cds-link-primary, #0f62fe)" stroke-width="2" points="%s" />`, pointList(points))
	for _, point := range points {
		fmt.Fprintf(&b, `<circle class="chart-point" cx="%s" cy="%s" r="3" fill="var(--cds-link-primary, #0f62fe)"><title>%s</title></circle>`, coord(point.X), coord(point.Y), escaped(humanBytes(point.Value)))
	}
	b.WriteString(`</svg>`)
	return template.HTML(b.String())
}

func stackedChart(title string, series map[string][]Sample, order []string) template.HTML {
	bands := stackedBandGeometry(series, order)

	var b strings.Builder
	fmt.Fprintf(&b, `<svg class="telemetry-chart telemetry-stacked-chart" viewBox="0 0 %.0f %.0f" role="img" aria-label="%s">`, chartWidth, chartHeight, escaped(title+" chart"))
	fmt.Fprintf(&b, `<title>%s</title>`, escaped(title))
	fmt.Fprintf(&b, `<text class="chart-heading" x="40" y="20">%s</text>`, escaped(title))
	fmt.Fprintf(&b, `<line class="chart-axis" x1="%s" y1="%s" x2="%s" y2="%s" stroke="var(--cds-border-subtle-01, #c6c6c6)" />`, coord(stackedPlotLeft), coord(stackedPlotBottom), coord(stackedPlotRight), coord(stackedPlotBottom))
	for _, band := range bands {
		fmt.Fprintf(&b, `<path class="chart-band" data-band="%s" d="%s" fill="%s" fill-opacity="0.85" stroke="%s" stroke-width="1"><title>%s</title></path>`, escaped(band.Name), bandPath(band), band.Color, band.Color, escaped(band.Name))
		fmt.Fprintf(&b, `<polyline class="chart-band-gap" fill="none" stroke="var(--cds-layer-01, #ffffff)" stroke-width="2" points="%s" />`, pointList(band.Top))
		fmt.Fprintf(&b, `<polyline class="chart-band-edge" fill="none" stroke="%s" stroke-width="1" points="%s" />`, band.Color, pointList(band.Top))
	}
	b.WriteString(`<g class="chart-legend">`)
	for i, band := range bands {
		x := 40.0 + float64(i%4)*148.0
		y := 196.0 + float64(i/4)*22.0
		fmt.Fprintf(&b, `<g class="chart-legend-item"><rect x="%s" y="%s" width="10" height="10" fill="%s"></rect><text x="%s" y="%s">%s</text></g>`, coord(x), coord(y-9), band.Color, coord(x+16), coord(y), escaped(band.Name))
	}
	b.WriteString(`</g></svg>`)
	return template.HTML(b.String())
}

func heroPointGeometry(samples []Sample, total int64) []chartPoint {
	if len(samples) == 0 {
		return []chartPoint{
			{X: heroPlotLeft, Y: heroPlotBottom},
			{X: heroPlotRight, Y: heroPlotBottom},
		}
	}

	points := make([]chartPoint, len(samples))
	for i, sample := range samples {
		x := heroPlotRight
		if len(samples) > 1 {
			x = heroPlotLeft + (heroPlotRight-heroPlotLeft)*float64(i)/float64(len(samples)-1)
		}
		points[i] = chartPoint{
			X:     x,
			Y:     scaledY(sample.Value, total, heroPlotTop, heroPlotBottom),
			Value: sample.Value,
		}
	}
	return points
}

func stackedBandGeometry(series map[string][]Sample, order []string) []stackedBand {
	ordered := orderedSeriesList(series, order)
	sort.SliceStable(ordered, func(i, j int) bool {
		return ordered[i].latest > ordered[j].latest
	})

	if len(ordered) > stackedTopBands {
		other := orderedSeries{name: "Other", values: foldValues(ordered[stackedTopBands:])}
		if len(other.values) > 0 {
			other.latest = other.values[len(other.values)-1]
		}
		ordered = append(append([]orderedSeries(nil), ordered[:stackedTopBands]...), other)
	}

	pointCount := maxPointCount(ordered)
	maxTotal := maxStackTotal(ordered, pointCount)
	if maxTotal == 0 {
		maxTotal = 1
	}

	bands := make([]stackedBand, len(ordered))
	cumulative := make([]int64, pointCount)
	for bandIndex, item := range ordered {
		band := stackedBand{
			Name:     item.name,
			Color:    bandPalette[bandIndex%len(bandPalette)],
			Values:   valuesAtLength(item.values, pointCount),
			StackTop: make([]int64, pointCount),
			Top:      make([]chartPoint, pointCount),
			Bottom:   make([]chartPoint, pointCount),
		}
		for i := 0; i < pointCount; i++ {
			bottomValue := cumulative[i]
			cumulative[i] += band.Values[i]
			band.StackTop[i] = cumulative[i]
			x := stackedX(i, pointCount)
			band.Bottom[i] = chartPoint{X: x, Y: scaledY(bottomValue, maxTotal, stackedPlotTop, stackedPlotBottom), Value: bottomValue}
			band.Top[i] = chartPoint{X: x, Y: scaledY(cumulative[i], maxTotal, stackedPlotTop, stackedPlotBottom), Value: cumulative[i]}
		}
		bands[bandIndex] = band
	}
	return bands
}

func orderedSeriesList(series map[string][]Sample, order []string) []orderedSeries {
	seen := make(map[string]bool, len(series))
	out := make([]orderedSeries, 0, len(series))
	for _, name := range order {
		if _, ok := series[name]; !ok || seen[name] {
			continue
		}
		seen[name] = true
		out = append(out, newOrderedSeries(name, series[name]))
	}

	remaining := make([]string, 0, len(series))
	for name := range series {
		if !seen[name] {
			remaining = append(remaining, name)
		}
	}
	sort.Strings(remaining)
	for _, name := range remaining {
		out = append(out, newOrderedSeries(name, series[name]))
	}
	return out
}

func newOrderedSeries(name string, samples []Sample) orderedSeries {
	values := make([]int64, len(samples))
	for i, sample := range samples {
		if sample.Value > 0 {
			values[i] = sample.Value
		}
	}
	latest := int64(0)
	if len(values) > 0 {
		latest = values[len(values)-1]
	}
	return orderedSeries{name: name, values: values, latest: latest}
}

func foldValues(items []orderedSeries) []int64 {
	pointCount := maxPointCount(items)
	values := make([]int64, pointCount)
	for _, item := range items {
		for i := 0; i < pointCount; i++ {
			if i < len(item.values) {
				values[i] += item.values[i]
			}
		}
	}
	return values
}

func maxPointCount(items []orderedSeries) int {
	pointCount := 0
	for _, item := range items {
		if len(item.values) > pointCount {
			pointCount = len(item.values)
		}
	}
	return pointCount
}

func maxStackTotal(items []orderedSeries, pointCount int) int64 {
	var maxTotal int64
	for i := 0; i < pointCount; i++ {
		var total int64
		for _, item := range items {
			if i < len(item.values) {
				total += item.values[i]
			}
		}
		if total > maxTotal {
			maxTotal = total
		}
	}
	return maxTotal
}

func valuesAtLength(values []int64, length int) []int64 {
	out := make([]int64, length)
	copy(out, values)
	return out
}

func stackedX(index, count int) float64 {
	if count <= 1 {
		return stackedPlotRight
	}
	return stackedPlotLeft + (stackedPlotRight-stackedPlotLeft)*float64(index)/float64(count-1)
}

func scaledY(value, total int64, top, bottom float64) float64 {
	if value <= 0 || total <= 0 {
		return bottom
	}
	ratio := float64(value) / float64(total)
	if ratio > 1 {
		ratio = 1
	}
	return bottom - (bottom-top)*ratio
}

func bandPath(band stackedBand) string {
	if len(band.Top) == 0 {
		return ""
	}
	var b strings.Builder
	fmt.Fprintf(&b, "M %s %s", coord(band.Top[0].X), coord(band.Top[0].Y))
	for _, point := range band.Top[1:] {
		fmt.Fprintf(&b, " L %s %s", coord(point.X), coord(point.Y))
	}
	for i := len(band.Bottom) - 1; i >= 0; i-- {
		point := band.Bottom[i]
		fmt.Fprintf(&b, " L %s %s", coord(point.X), coord(point.Y))
	}
	b.WriteString(" Z")
	return b.String()
}

func pointList(points []chartPoint) string {
	parts := make([]string, len(points))
	for i, point := range points {
		parts[i] = coord(point.X) + "," + coord(point.Y)
	}
	return strings.Join(parts, " ")
}

func coord(v float64) string {
	return fmt.Sprintf("%.1f", v)
}

func escaped(s string) string {
	return html.EscapeString(s)
}
