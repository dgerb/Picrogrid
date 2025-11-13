# import pandas as pd
# import numpy as np
# import matplotlib.pyplot as plt

# def enforce_monotonic(x, y):
#     """Ensure y is monotonic (increasing or decreasing) with x."""
#     x = np.array(x)
#     y = np.array(y)
#     sort_idx = np.argsort(x)
#     x, y = x[sort_idx], y[sort_idx]

#     # Determine global trend
#     trend = np.sign(y[-1] - y[0])  # +1 increasing, -1 decreasing, 0 flat

#     if trend == 0:
#         raise ValueError("Data has no variation; cannot determine monotonic direction.")

#     keep_mask = np.ones_like(y, dtype=bool)
#     for i in range(1, len(y)):
#         if trend > 0 and y[i] <= y[i - 1]:
#             keep_mask[i] = False
#         elif trend < 0 and y[i] >= y[i - 1]:
#             keep_mask[i] = False

#     return x[keep_mask], y[keep_mask]

# def select_representative_points(x, y, n_points=10):
#     """Select n_points minimizing linear interpolation error."""
#     x = np.array(x)
#     y = np.array(y)
#     sort_idx = np.argsort(x)
#     x, y = x[sort_idx], y[sort_idx]
#     selected = [0, len(x) - 1]  # always include endpoints

#     while len(selected) < n_points and len(selected) < len(x):
#         selected.sort()
#         interp_y = np.interp(x, x[selected], y[selected])
#         errors = np.abs(y - interp_y)
#         errors[selected] = -1  # ignore already selected points
#         idx = np.argmax(errors)
#         selected.append(idx)

#     selected.sort()
#     return x[selected], y[selected]

# def create_interpolation_table(
#     excel_path,
#     sheet_name=0,
#     x_col=None,
#     y_col=None,
#     n_points=10,
#     output_path=None,
#     enforce_one_to_one=True,
#     plot=False,
# ):
#     """Create reduced lookup table with optional monotonic constraint."""
#     df = pd.read_excel(excel_path, sheet_name=sheet_name, header=0)

#     # Auto-detect columns if not provided
#     if x_col is None or y_col is None:
#         if df.shape[1] < 2:
#             raise ValueError("Excel sheet must contain at least two columns.")
#         x_col = df.columns[0] if x_col is None else x_col
#         y_col = df.columns[1] if y_col is None else y_col

#     x = df[x_col].dropna().to_numpy()
#     y = df[y_col].dropna().to_numpy()

#     if len(x) != len(y):
#         raise ValueError("x and y columns must have the same length.")

#     # Enforce injectivity
#     if enforce_one_to_one:
#         x, y = enforce_monotonic(x, y)

#     # Select representative points
#     x_sel, y_sel = select_representative_points(x, y, n_points)
#     table = pd.DataFrame({x_col: x_sel, y_col: y_sel})

#     if output_path:
#         table.to_excel(output_path, index=False)
#         print(f"Saved lookup table to {output_path}")

#     if plot:
#         plt.figure(figsize=(7, 4))
#         plt.plot(x, y, "o-", label="Original (filtered)" if enforce_one_to_one else "Original")
#         plt.plot(x_sel, y_sel, "ro-", label=f"{len(x_sel)} representative points")
#         plt.xlabel(x_col)
#         plt.ylabel(y_col)
#         plt.legend()
#         plt.title("Monotonic Lookup Table (Injective Constraint)")
#         plt.grid(True)
#         plt.tight_layout()
#         plt.show()

#     return table

# # Example usage
# if __name__ == "__main__":
#     table = create_interpolation_table(
#         "TimeUSB_50Ah.xlsx",
#         x_col="SOC %",
#         y_col="VBus @SS",
#         n_points=8,
#         enforce_one_to_one=True,
#         plot=True,
#         output_path="lookup_table_injective.xlsx",
#     )
#     print(table)




import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def select_representative_points(x, y, n_points=10):
    """
    Greedy error-based subsampling:
    start with endpoints, iteratively add the point with max interpolation error.
    Returns (x_subset, y_subset) sorted by x.
    """
    x = np.asarray(x)
    y = np.asarray(y)
    sort_idx = np.argsort(x)
    x, y = x[sort_idx], y[sort_idx]

    if len(x) < 2:
        raise ValueError("Need at least two data points.")

    selected = [0, len(x) - 1]  # include endpoints

    while len(selected) < min(n_points, len(x)):
        selected.sort()
        interp_y = np.interp(x, x[selected], y[selected])
        errors = np.abs(y - interp_y)
        errors[selected] = -1  # ignore already selected
        idx = np.argmax(errors)
        selected.append(idx)

    selected.sort()
    return x[selected], y[selected]

def _get_series(df, col):
    """Return a pandas Series by name or index."""
    if isinstance(col, int):
        return df.iloc[:, col]
    return df[col]

def create_interpolation_table(
    excel_path,
    sheet_name=0,
    x_col=None,            # name or index
    y_col=None,            # name or index
    n_points=10,
    output_path=None,
    plot=False,
):
    """
    1) Load (x,y) from Excel (choose columns by name or index)
    2) Select n representative points
    3) Round selected x's to nearest integers
    4) Interpolate y at those integer x's using the original curve
    5) Return/optionally save the integer-x lookup table
    """
    # Load
    df = pd.read_excel(excel_path, sheet_name=sheet_name, header=0)

    # Default to first two columns if not provided
    if x_col is None or y_col is None:
        if df.shape[1] < 2:
            raise ValueError("Excel sheet must contain at least two columns.")
        x_col = df.columns[0] if x_col is None else x_col
        y_col = df.columns[1] if y_col is None else y_col

    xs = _get_series(df, x_col)
    ys = _get_series(df, y_col)

    # Drop rows where either is NaN, keep aligned indices
    clean = pd.concat([xs, ys], axis=1).dropna()
    x_all = clean.iloc[:, 0].to_numpy()
    y_all = clean.iloc[:, 1].to_numpy()

    # Sort original data (for robust interpolation)
    sort_idx = np.argsort(x_all)
    x_all = x_all[sort_idx]
    y_all = y_all[sort_idx]

    # Select representative points (float x)
    x_sel, y_sel = select_representative_points(x_all, y_all, n_points=n_points)

    # Round the selected x's to integers
    x_int = np.rint(x_sel).astype(int)

    # Deduplicate integer x's while preserving order of appearance
    uniq_x_int, uniq_indices = np.unique(x_int, return_index=True)
    x_int_unique = x_int[np.sort(uniq_indices)]

    # Interpolate y at those integer x's using the ORIGINAL curve (x_all, y_all)
    # np.interp clamps out-of-range to endpoints by default, which is often desired
    y_int = np.interp(x_int_unique, x_all, y_all)
    y_int = y_int.astype(np.int32)

    # Build result table (keep original column names if they’re strings)
    x_name = x_col if isinstance(x_col, str) else str(df.columns[x_col])
    y_name = y_col if isinstance(y_col, str) else str(df.columns[y_col])
    table = pd.DataFrame({x_name: x_int_unique, y_name: y_int})

    # Optional save
    if output_path:
        table.to_excel(output_path, index=False)
        print(f"Saved integer-x lookup table to {output_path}")

    # Optional plot for quick validation
    if plot:
        plt.figure(figsize=(7, 4))
        plt.plot(x_all, y_all, "o-", alpha=0.5, label="Original data")
        plt.plot(x_sel, y_sel, "kx--", label=f"Selected {len(x_sel)} pts (pre-round)")
        plt.plot(x_int_unique, y_int, "ro-", label="Rounded X + re-interpolated Y")
        plt.xlabel(x_name)
        plt.ylabel(y_name)
        plt.grid(True)
        plt.legend()
        plt.tight_layout()
        plt.show()

    return table

# Example usage:
if __name__ == "__main__":
    out = create_interpolation_table(
        "TimeUSB_50Ah.xlsx",
        sheet_name=0,
        x_col="SOC %",
        y_col="VBus @SS",
        n_points=10,
        output_path="lookup_table_integer_x.xlsx",
        plot=True,
    )
    print(out)
